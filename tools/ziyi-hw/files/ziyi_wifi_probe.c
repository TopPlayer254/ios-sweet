/* SPDX-License-Identifier: MIT
 * Ziyi opt-in in-system RO WPSS survey. No radio/power/PAS/DMA or media writes.
 */
#include <pexpert/arm64/board_config.h>
#if SWEET
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/proc_internal.h>
#include <kern/clock.h>
#include <kern/thread.h>
#include <kern/sched_prim.h>
#include <machine/machine_routines.h>
#include <vm/memory_types.h>
#include <arm/memory_types.h>
#include <mach/vm_prot.h>
#include <pexpert/pexpert.h>
#include <pexpert/device_tree.h>
#include "ziyi_wifi_core.h"
#include "ziyi_wifi_sha.inc"
#include "ziyi_wifi_read_gate.inc"
#include "ziyi_wifi_smem.inc"
#include "ziyi_wifi_gate.inc"
#include "ziyi_wifi_survey.inc"
/* Exact U25 exported private API: io_map(...,prot,unmappable). */
extern vm_offset_t io_map(vm_map_offset_t,vm_size_t,unsigned int,vm_prot_t,bool);
extern void ml_io_unmap(vm_offset_t,vm_size_t);
extern kern_return_t thread_terminate(thread_t);
void ziyi_wifi_probe_start(void);
#define ZWIFI_SYSREG(reg) ({uint64_t v;__asm__ volatile("mrs %0," #reg:"=r"(v));v;})
static struct zwifi_record zwifi_last;
static struct zwifi_cursor zwifi_cursor;
static vm_offset_t zwifi_maps[4];
static vm_size_t zwifi_map_bytes[4];
static unsigned zwifi_once;
static int zwifi_channel;
static void zwifi_sleep(unsigned ms){uint64_t end;clock_interval_to_deadline(ms,NSEC_PER_MSEC,&end);(void)assert_wait_deadline(&zwifi_channel,THREAD_UNINT,end);thread_block(THREAD_CONTINUE_NULL);}
static int zwifi_equal(const void*aa,const void*bb,unsigned n){const uint8_t*a=aa,*b=bb;for(unsigned i=0;i<n;i++)if(a[i]!=b[i])return 0;return 1;}
static int zwifi_adt(void){
 struct zwifi_record*r=&zwifi_last;DTEntry node;const void*value;unsigned n;r->adt_facts=0;
 if(!SecureDTIsLockedDown()||SecureDTLookupEntry(NULL,"/arm-io/ziyi-wifi-probe",&node)!=0)return 0;r->adt_facts|=1|32;
 static const char compatible[]="ziyi,wcn6750-ro-probe";
 if(SecureDTGetProperty(node,"compatible",&value,&n)!=0||n!=sizeof(compatible)||!zwifi_equal(value,compatible,n))return 0;r->adt_facts|=2;
 if(SecureDTGetProperty(node,"contract",&value,&n)!=0||n!=sizeof(r->contract))return 0;bcopy(value,&r->contract,n);
 if(!zwifi_contract_valid(&r->contract))return 0;r->adt_facts|=4;
 if(SecureDTGetProperty(node,"reg",&value,&n)!=0||n!=sizeof(r->contract.region)||!zwifi_equal(value,r->contract.region,n))return 0;r->adt_facts|=8;
 if(SecureDTGetProperty(node,"source-digest",&value,&n)!=0||n!=32||!zwifi_equal(value,r->contract.source_digest,32))return 0;r->adt_facts|=16;
 return r->adt_facts==ZWIFI_ADT_FACTS;
}
static int zwifi_context_now(void){return ZWIFI_SYSREG(CurrentEL)==4&&(ZWIFI_SYSREG(sctlr_el1)&0x1005)==0x1005&&ZWIFI_SYSREG(cntfrq_el0)==19200000;}
static int zwifi_load(uint64_t physical,void*to,uint32_t bytes){
 struct zwifi_record*r=&zwifi_last;if(!zwifi_context_now())return 0;
 for(unsigned region=0;region<4;region++){
  uint64_t base=r->contract.region[region].physical,size=r->contract.region[region].bytes;
  if(!zwifi_maps[region]||physical<base||physical-base>size||bytes>size-(physical-base))continue;
  /* The policy admits only aligned32bit reads. No memcpy or vector access to Device MMIO. */
  if((physical&3)||(bytes&3))return 0;uint8_t*dest=to;vm_offset_t va=zwifi_maps[region]+(vm_offset_t)(physical-base);
  for(unsigned i=0;i<bytes;i+=4){uint32_t v=*(volatile const uint32_t*)(va+i);for(unsigned j=0;j<4;j++)dest[i+j]=(uint8_t)(v>>(8*j));}
  __asm__ volatile("dsb sy":::"memory");return 1;
 }
 return 0;
}
static int zwifi_smem_read(void*ctx,uint32_t off,void*to,uint32_t bytes){
 (void)ctx;struct zwifi_record*r=&zwifi_last;uint64_t physical=ZW_SMEM_BASE+off;
 if(!zwifi_address_allowed(r,physical,bytes)){r->denied++;return 0;}
 if(!zwifi_load(physical,to,bytes))return 0;
 if(off==0x5c&&bytes==4){uint8_t*p=to;unsigned pass=r->smem_version[0]?1:0;r->smem_version[pass]=(uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
 return 1;
}
static int zwifi_survey_read(void*ctx,uint64_t physical,void*to,uint32_t bytes){
 (void)ctx;if(!zwifi_survey_allowed(&zwifi_last,&zwifi_cursor,physical,bytes)){zwifi_last.denied++;return 0;}return zwifi_load(physical,to,bytes);
}
static int zwifi_springboard(proc_t p,void*ctx){const char*name=proc_best_name(p);if(name&&strcmp(name,"SpringBoard")==0){*(int*)ctx=proc_pid(p);return PROC_RETURNED_DONE;}return PROC_RETURNED;}
static void zwifi_hash_line(const char*what,unsigned part,const uint8_t*d){static const char digits[]="0123456789abcdef";char hex[65];for(unsigned i=0;i<32;i++){hex[2*i]=digits[d[i]>>4];hex[2*i+1]=digits[d[i]&15];}hex[64]=0;printf("ZWIFI RO %s p%u %s\n",what,part,hex);zwifi_sleep(250);}
static void zwifi_summary(int pid){
 struct zwifi_record*r=&zwifi_last;printf("ZWIFI RO kernel sampler SBpid%d status%u stage%u crc%08x bytes%u\n",pid,r->status,r->stage,r->crc,r->bytes);zwifi_sleep(250);
 printf("ZWIFI RO ADT%02x EL%llu MCI%llx counter%llu rawSBL%08x/%08x\n",r->adt_facts,r->el,r->sctlr&0x1005,r->frequency,r->smem_version[0],r->smem_version[1]);zwifi_sleep(250);
 printf("ZWIFI RO table%u reads%u WPSS%u global%u extra%u deny%u flags%x\n",r->smem.count,r->smem.reads,r->smem.wpss_entry,r->smem.global_entry,r->extra_reads,r->denied,r->survey_error);zwifi_sleep(250);
 for(unsigned part=0;part<2;part++){
  struct zwifi_walk*w=&r->walk[0][part];printf("ZWIFI RO chain%u err%u/%u n%u u%u c%u lane%u end%x stable%d\n",part,w->error,r->walk[1][part].error,w->count,w->uncached_count,w->cached_count,w->end_lane,w->end_at,zwifi_equal(w,&r->walk[1][part],sizeof(*w)));zwifi_sleep(250);zwifi_hash_line("metadataSHA256",part,w->sha256);
 }
 for(unsigned t=0;t<3;t++){
  struct zwifi_item*i=&r->item[0][t];unsigned id=t==0?616:t==1?617:626;uint8_t hash[32];zwifi_sha256(i->data,i->captured<=340?i->captured:340,hash);
  printf("ZWIFI RO item%u found%u part%u off%x len%u cap%u stable%d\n",id,i->found,i->part,i->offset,i->logical,i->captured,zwifi_equal(i,&r->item[1][t],sizeof(*i)));zwifi_sleep(250);zwifi_hash_line("payloadSHA256",id,hash);
  if(i->found&&t<2){unsigned valid=zc_u16(i->data+14);printf("ZWIFI RO SMP%u v%u %u->%u entries%u flags%08x retained-state-only\n",id,zc_u32(i->data+4)&255,zc_u16(i->data+8),zc_u16(i->data+10),valid,zc_u32(i->data+16));zwifi_sleep(250);for(unsigned n=0;n<valid&&n<16;n++){char name[17];bcopy(i->data+20+20*n,name,16);name[16]=0;printf("ZWIFI RO SMP%u %.16s=%08x\n",id,name,zc_u32(i->data+36+20*n));zwifi_sleep(250);}}
 }
 printf("ZWIFI RO QMPerr%u/%u states%08x/%08x boxlen%u/%u IPCC%08x/%08x\n",r->qmp[0].error,r->qmp[1].error,r->qmp[0].descriptor[9],r->qmp[0].descriptor[11],r->qmp[0].length,r->qmp[1].length,r->ipcc_pending[0],r->ipcc_pending[1]);zwifi_sleep(250);
 printf("ZWIFI RO CMDerr%u/%u cx%x mx%x calls%u/%u, no-radio/no-scan\n",r->db[0].error,r->db[1].error,r->db[0].entry[0][4],r->db[0].entry[1][4],r->db[0].reads,r->db[1].reads);zwifi_sleep(250);
 printf("ZWIFI RO DONE crc%08x complete%u; no ACK/alloc/PAS/power/DMA/media writes\n",r->crc,r->survey_complete);
}
static void zwifi_thread(void*unused,wait_result_t wait){
 (void)unused;(void)wait;struct zwifi_record*r=&zwifi_last;unsigned delay_seconds=60;int pid=0;r->version=1;r->bytes=sizeof(*r);
 PE_parse_boot_argn("ziyi_wifi_probe_delay",&delay_seconds,sizeof(delay_seconds));if(delay_seconds>300)delay_seconds=300;
 printf("ZWIFI RO armed opt-in, delay%us then await SpringBoard; radio remains unstarted\n",delay_seconds);zwifi_sleep(delay_seconds*1000u);
 for(unsigned n=0;n<60&&!pid;n++){proc_iterate(PROC_ALLPROCLIST|PROC_NOWAITTRANS,zwifi_springboard,&pid,NULL,NULL);if(!pid)zwifi_sleep(5000);}
 if(!pid){r->status=1;printf("ZWIFI RO STOP SpringBoard absent after bounded wait; no device maps\n");goto out;}
 zwifi_sleep(5000);r->start=ZWIFI_SYSREG(cntvct_el0);r->mpidr_start=ZWIFI_SYSREG(mpidr_el1);r->el=ZWIFI_SYSREG(CurrentEL);r->sctlr=ZWIFI_SYSREG(sctlr_el1);r->frequency=ZWIFI_SYSREG(cntfrq_el0);
 if(!zwifi_adt()||!zwifi_context_valid(r)){r->status=2;printf("ZWIFI RO STOP missing/invalid locked ADT contract/context, facts%x; no device maps\n",r->adt_facts);goto out;}
 for(unsigned n=0;n<4;n++){
  uint64_t physical=r->contract.region[n].physical,size=r->contract.region[n].bytes;zwifi_map_bytes[n]=round_page((vm_size_t)size);
  zwifi_maps[n]=io_map(physical,(vm_size_t)size,VM_WIMG_IO,VM_PROT_READ,true);
  if(!zwifi_maps[n]){r->status=3;goto out;}
 }
 if(!zwifi_adt()||!zwifi_context_now()){r->status=4;goto out;}
 r->stage=3;int success=zw_smem_observe(zwifi_smem_read,NULL,&r->smem);
 if(success&&(r->smem_version[0]!=r->smem_version[1]||r->smem_version[0]>>16!=12)){r->smem.error=ZW_CHANGED;success=0;}
 r->stage=5;int complete=zwifi_survey(r,&zwifi_cursor,zwifi_survey_read,NULL);r->stage=6;
 if(!success)r->status=2000+r->smem.error;else if(!complete)r->status=3000;
 if(!zwifi_adt())r->status=5;
 out:
 __asm__ volatile("dsb sy":::"memory");for(unsigned n=0;n<4;n++)if(zwifi_maps[n]){ml_io_unmap(zwifi_maps[n],zwifi_map_bytes[n]);zwifi_maps[n]=0;}
 r->end=ZWIFI_SYSREG(cntvct_el0);r->mpidr_end=ZWIFI_SYSREG(mpidr_el1);r->crc=0;r->crc=zwifi_crc32(r,sizeof(*r));zwifi_summary(pid);
 (void)thread_terminate(current_thread());thread_block(THREAD_CONTINUE_NULL);
}
void ziyi_wifi_probe_start(void){
 int enabled=0;if(!PE_parse_boot_argn("ziyi_wifi_probe",&enabled,sizeof(enabled))||enabled!=1)return;
 unsigned expected=0;if(!__atomic_compare_exchange_n(&zwifi_once,&expected,1,false,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED))return;
 thread_t thread;kern_return_t error=kernel_thread_start(zwifi_thread,NULL,&thread);if(error==KERN_SUCCESS)thread_deallocate(thread);else printf("ZWIFI RO STOP thread start %d\n",error);
}
#else
#error "ziyi_wifi_probe requires the SWEET kernel profile"
#endif
