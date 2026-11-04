/* SPDX-License-Identifier: GPL-2.0-only */
/* Experimental XNU integration of the guarded G6/G7 acquisition engines.
 * Opt-in G8 FW_INIT100 transaction; no HFI start, CP or acceleration API.
 * ARM64 ml_probe_read is unimplemented in this XNU. Device loads below may
 * synchronously abort/panic; no post-fault restoration is asserted. */
#include <pexpert/arm64/board_config.h>
#if !SWEET
#error "ziyi_gpu_probe requires the SWEET kernel profile"
#endif
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/proc_internal.h>
#include <kern/clock.h>
#include <kern/thread.h>
#include <kern/sched_prim.h>
#include <mach/vm_param.h>
#include <mach/vm_prot.h>
#include <vm/memory_types.h>
#include <arm/memory_types.h>
#include <machine/machine_routines.h>
#include <pexpert/pexpert.h>
#include <pexpert/device_tree.h>
#include "ziyi_gpu_probe.h"
#include "ziyi_gpu_probe_contract.h"
#include "ziyi_g7_clock.h"
#include "ziyi_g7_engine.h"
#include "ziyi_g8_fwinit.h"
#include <arm/caches_internal.h>
#include "ziyi_adt_binding.h"
extern uint64_t kvtophys(vm_offset_t);
#include "ziyi_gpu_adt_properties.inc"
extern vm_offset_t io_map(vm_map_offset_t,vm_size_t,unsigned int,vm_prot_t,bool);
extern void ml_io_unmap(vm_offset_t,vm_size_t);
extern kern_return_t thread_terminate(thread_t);
#define ZG_RW_MAPS 64u
struct zg_mapping {struct zg_region region;struct zg_geometry geometry;vm_offset_t page_va;};
struct zg_state {
 struct zadt_binding adt;uint32_t checked_clock_phase,checked_probe_phase,checked_g8_phase,gmu_enabled;
 struct g8_workspace firmware;
 struct zg_contract contract;struct zg_mapping ro[ZG_REGIONS],rw[ZG_RW_MAPS];unsigned rw_count;
 struct g7c_record clocks;struct g7_record probe;uint64_t lease_epoch;uint32_t lease;
 uint8_t copy0[G7_TCM_BYTES] __attribute__((aligned(64)));
 uint8_t copy1[G7_TCM_BYTES] __attribute__((aligned(64)));
};
static struct zg_state zg;
static uint32_t zg_started;
static int zg_wait_channel;
static void zg_sleep(unsigned ms){uint64_t end;clock_interval_to_deadline(ms,NSEC_PER_MSEC,&end);(void)assert_wait_deadline(&zg_wait_channel,THREAD_UNINT,end);thread_block(THREAD_CONTINUE_NULL);}
static int zg_springboard(proc_t p,void*ctx){const char*name=proc_best_name(p);if(name&&strcmp(name,"SpringBoard")==0){*(int*)ctx=proc_pid(p);return PROC_RETURNED_DONE;}return PROC_RETURNED;}
static void zg_barrier(void*ctx){(void)ctx;__asm__ volatile("dsb sy":::"memory");}
static uint64_t zg_ticks(void*ctx){(void)ctx;uint64_t v;__asm__ volatile("mrs %0,cntvct_el0":"=r"(v));return v;}
static uint64_t zg_sctlr(void){uint64_t v;__asm__ volatile("mrs %0,sctlr_el1":"=r"(v));return v;}
static uint64_t zg_el(void){uint64_t v;__asm__ volatile("mrs %0,CurrentEL":"=r"(v));return v;}
static uint64_t zg_frequency(void){uint64_t v;__asm__ volatile("mrs %0,cntfrq_el0":"=r"(v));return v;}
static void zg_delay(void*ctx,uint32_t us){uint64_t first=zg_ticks(ctx),ticks=((uint64_t)us*192+9)/10;while(zg_ticks(ctx)-first<ticks)__asm__ volatile("yield");}
static int zg_get(void*ctx,const char*name,const void**data,unsigned*bytes){return SecureDTGetProperty((DTEntry)ctx,name,data,bytes)==kSuccess;}
static int zg_adt_check(struct zg_state*s,int first){
 int ok=first?zadt_open(&s->adt,"/arm-io/ziyi-gpu-probe",zg_adt_properties,ZG_ADT_PROPERTY_COUNT):zadt_recheck(&s->adt,"/arm-io/ziyi-gpu-probe",zg_adt_properties,ZG_ADT_PROPERTY_COUNT);
 if(!ok)return 0;struct zg_contract fresh;
 if(!zg_contract_read(&fresh,zg_get,(void*)s->adt.entry))return 0;
 if(first)s->contract=fresh;else if(!zadt_equal(&fresh,&s->contract,sizeof(fresh)))return 0;
 return 1;
}
static int zg_map_one(struct zg_mapping*m,struct zg_region region,vm_prot_t prot){
 struct zg_geometry geometry;if(!zg_map_geometry(region.base,region.bytes,PAGE_SIZE,&geometry))return 0;
 vm_offset_t va=io_map((vm_map_offset_t)geometry.page_base,(vm_size_t)geometry.page_span,VM_WIMG_IO,prot,true);
 if(!va)return 0;m->region=region;m->geometry=geometry;m->page_va=va;return 1;
}
static void zg_unmap_all(struct zg_state*s){
 for(unsigned i=0;i<s->rw_count;i++)if(s->rw[i].page_va){ml_io_unmap(s->rw[i].page_va,(vm_size_t)s->rw[i].geometry.page_span);s->rw[i].page_va=0;}
 for(unsigned i=0;i<ZG_REGIONS;i++)if(s->ro[i].page_va){ml_io_unmap(s->ro[i].page_va,(vm_size_t)s->ro[i].geometry.page_span);s->ro[i].page_va=0;}
}
static int zg_map_all(struct zg_state*s){
 for(unsigned i=0;i<ZG_REGIONS;i++)if(!zg_map_one(&s->ro[i],s->contract.regions[i],VM_PROT_READ))return 0;
 for(unsigned i=0;i<G2_NREGS;i++){
  const struct g7c_reg*d=&g7c_regs[i];if(!g7c_write_allowed(d->base,d->offset))continue;
  uint64_t address=d->base+d->offset;struct zg_geometry g;if(!zg_map_geometry(address,4,PAGE_SIZE,&g))return 0;
  unsigned n;for(n=0;n<s->rw_count;n++)if(s->rw[n].geometry.page_base==g.page_base)break;
  if(n<s->rw_count)continue;if(s->rw_count==ZG_RW_MAPS)return 0;
  if(!zg_map_one(&s->rw[s->rw_count],(struct zg_region){g.page_base,g.page_span},VM_PROT_READ|VM_PROT_WRITE))return 0;
  s->rw_count++;
 }
 if(s->gmu_enabled){
  /* MMIO page coverage is not authorization; native callback independently
   * gates exact widths/addresses/fields immediately before every store. */
  for(unsigned i=0;i<G8_REGS+1;i++){
   struct zg_geometry g;if(!zg_map_geometry(i<G8_REGS?g8_regs[i].address:0x3d6d000,i<G8_REGS?g8_regs[i].width:0x8000,PAGE_SIZE,&g))return 0;
   /* Full TCM span can cross several 16K pages; keep one owned IO mapping. */
   if(s->rw_count==ZG_RW_MAPS)return 0;
   if(!zg_map_one(&s->rw[s->rw_count],(struct zg_region){g.page_base,g.page_span},VM_PROT_READ|VM_PROT_WRITE))return 0;s->rw_count++;
  }
 }
 return 1;
}
static volatile uint32_t*zg_pointer(struct zg_mapping*m,unsigned count,uint64_t address){
 if(address&3)return 0;
 for(unsigned i=0;i<count;i++){uint64_t offset;if(m[i].page_va&&zg_region_offset(&m[i].region,address,4,&offset))return (volatile uint32_t*)(uintptr_t)(m[i].page_va+m[i].geometry.bias+offset);}
 return 0;
}
static uint32_t zg_read_clock(void*ctx,uint64_t base,uint32_t off,int*ok){
 struct zg_state*s=ctx;*ok=0;if(s->checked_clock_phase!=s->clocks.phase){if(!zg_adt_check(s,0)){s->clocks.poison=1;return 0;}s->checked_clock_phase=s->clocks.phase;}if(!g7c_read_allowed(&s->clocks,base,off)||base>UINT64_MAX-off)return 0;
 volatile uint32_t*p=zg_pointer(s->ro,ZG_REGIONS,base+off);if(!p)return 0;
 uint32_t value=*p;zg_barrier(s);*ok=1;return value;
}
static void zg_write_clock(void*ctx,uint64_t base,uint32_t off,uint32_t value,int*ok){
 struct zg_state*s=ctx;*ok=0;if(!zg_adt_check(s,0)){s->clocks.poison=1;return;}if(!g7c_write_allowed(base,off)||base>UINT64_MAX-off)return;
 volatile uint32_t*read=zg_pointer(s->ro,ZG_REGIONS,base+off),*write=zg_pointer(s->rw,s->rw_count,base+off);if(!read||!write)return;
 uint32_t now=*read;zg_barrier(s);for(unsigned i=0;i<G2_NREGS;i++)if(g7c_regs[i].base==base&&g7c_regs[i].offset==off)s->clocks.final[i]=now;
 if(!g7c_store_guard(&s->clocks,base,off,value,now))return;
 *write=value;zg_barrier(s);*ok=1;
}
static uint32_t zg_read_probe(void*ctx,uint64_t address,int*ok){
 struct zg_state*s=ctx;*ok=0;if(s->checked_probe_phase!=s->probe.phase){if(!zg_adt_check(s,0)){s->probe.poison=1;return 0;}s->checked_probe_phase=s->probe.phase;}if(!g7_read_allowed(&s->probe,address))return 0;
 volatile uint32_t*p=zg_pointer(s->ro,ZG_REGIONS,address);if(!p)return 0;uint32_t value=*p;zg_barrier(s);*ok=1;return value;
}
static int zg_lease(void*ctx,uint64_t epoch,int acquire){
 struct zg_state*s=ctx;if(!epoch)return 0;
 if(acquire){uint32_t expected=0;if(!__atomic_compare_exchange_n(&s->lease,&expected,1,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 0;s->lease_epoch=epoch;return 1;}
 if(__atomic_load_n(&s->lease,__ATOMIC_ACQUIRE)!=1||s->lease_epoch!=epoch)return 0;s->lease_epoch=0;__atomic_store_n(&s->lease,0,__ATOMIC_RELEASE);return 1;
}
static int zg_owned(void*ctx,const void*memory,size_t bytes){struct zg_state*s=ctx;return bytes==G7_TCM_BYTES&&(memory==s->copy0||memory==s->copy1);}
static void zg_phase(void*ctx,uint32_t phase){(void)ctx;printf("ZIYI GPU clock phase %u\n",phase);}
static void*zg_g8_pointer(struct zg_state*s,uint64_t at,unsigned width,int write){
 if((width!=4&&width!=8)||(at&(width-1)))return NULL;
 /* MMIO only. Firmware and page-table RAM use ordinary static BSS, never IO aliases. */
 for(unsigned pass=0;pass<(write?1u:2u);pass++){struct zg_mapping*m=pass?s->ro:s->rw;unsigned n=pass?ZG_REGIONS:s->rw_count;
  for(unsigned i=0;i<n;i++){uint64_t off;if(m[i].page_va&&zg_region_offset(&m[i].region,at,width,&off))return(void*)(uintptr_t)(m[i].page_va+m[i].geometry.bias+off);}}
 return NULL;
}
static uint64_t zg_g8_load(struct zg_state*s,uint64_t at,unsigned width,int*ok){void*p=zg_g8_pointer(s,at,width,0);*ok=p!=NULL;if(!p)return 0;uint64_t value=width==8?*(volatile uint64_t*)p:*(volatile uint32_t*)p;zg_barrier(s);return value;}
static uint64_t zg_g8_read(void*ctx,uint64_t at,unsigned width,int*ok){struct zg_state*s=ctx;*ok=0;
 if(s->checked_g8_phase!=s->firmware.record.phase){if(!zg_adt_check(s,0)){s->firmware.record.poison=1;return 0;}s->checked_g8_phase=s->firmware.record.phase;printf("ZIYI GMU stage%u flags%x writes%u FW%x\n",s->checked_g8_phase,s->firmware.record.flags,s->firmware.record.writes,(unsigned)s->firmware.record.expected[3]);}
 if(!s->gmu_enabled||!g8_read_allowed(&s->firmware,at,width))return 0;return zg_g8_load(s,at,width,ok);
}
static void zg_g8_write(void*ctx,uint64_t at,unsigned width,uint64_t value,int*ok){struct zg_state*s=ctx;*ok=0;
 if(!s->gmu_enabled||!zg_adt_check(s,0)||__atomic_load_n(&s->lease,__ATOMIC_ACQUIRE)!=1||s->lease_epoch!=s->firmware.record.epoch)return;
 uint64_t observe=0;int mode=g8_observation_address(at,width,&observe),good=1;if(mode<0)return;uint64_t now=mode?zg_g8_load(s,observe,width,&good):0;if(!good)return;
 uint32_t reset=(uint32_t)zg_g8_load(s,GD_RESET,4,&good);if(!good)return;uint32_t pending=0;
 if(s->firmware.record.phase==G8_CONFIG_RESTORE&&(at==0x3d7e64c||at==0x3d8ec18)){pending=(uint32_t)zg_g8_load(s,at==0x3d7e64c?0x3d7e648:0x3d8ec14,4,&good);if(!good)return;}
 if(s->firmware.record.phase==G9_RELEASE_BUS&&at==GD_HALT){pending=(uint32_t)zg_g8_load(s,GD_ACK,4,&good);if(!good)return;}
 /* Fresh native HALT/ACK observations immediately before CM3 store. */
 if(s->firmware.record.phase==G8_START&&at==GD_RESET){uint32_t halt=(uint32_t)zg_g8_load(s,GD_HALT,4,&good);if(!good||halt)return;pending=(uint32_t)zg_g8_load(s,GD_ACK,4,&good);if(!good)return;}
 if(!g8_store_guard(&s->firmware,at,width,value,now,reset,pending))return;void*p=zg_g8_pointer(s,at,width,1);if(!p)return;
 if(width==8)*(volatile uint64_t*)p=value;else *(volatile uint32_t*)p=(uint32_t)value;zg_barrier(s);*ok=1;
}
static int zg_g8_owned(void*ctx,const void*memory,size_t n){struct zg_state*s=ctx;return memory==&s->firmware&&n==sizeof(s->firmware);}
static uint64_t zg_g8_physical(void*ctx,const void*va){struct zg_state*s=ctx;uintptr_t at=(uintptr_t)va,first=(uintptr_t)&s->firmware.memory;
 if(at<first||at-first>=sizeof(s->firmware.memory)||(at&4095))return 0;return kvtophys((vm_offset_t)va);}
static int zg_g8_cache_span(struct zg_state*s,void*va,size_t n){uintptr_t at=(uintptr_t)va,first=(uintptr_t)&s->firmware.memory;return n&&at>=first&&at-first<sizeof(s->firmware.memory)&&n<=sizeof(s->firmware.memory)-(at-first);}
static int zg_g8_flush(void*ctx,void*va,size_t n){struct zg_state*s=ctx;if(!zg_g8_cache_span(s,va,n))return 0;FlushPoC_DcacheRegion((vm_offset_t)va,n);return 1;}
static int zg_g8_clean(void*ctx,void*va,size_t n){struct zg_state*s=ctx;if(!zg_g8_cache_span(s,va,n))return 0;CleanPoC_DcacheRegion_Force((vm_offset_t)va,n);return 1;}
static int zg_g8_proof(void*ctx,const struct g8_prereq*q,const struct g8_blob*b){struct zg_state*s=ctx;struct g8_blob actual;
 if(!s->gmu_enabled||b!=&s->firmware.blob||!zg_adt_check(s,0)||q->board!=s->contract.board||q->hlos_cb!=s->contract.cb||q->hlos_smr!=s->contract.smr||q->current_el!=zg_el()||q->sctlr!=zg_sctlr()||s->clocks.flags!=127||s->clocks.error||s->clocks.poison)return 0;
 for(unsigned i=0;i<4;i++)if(q->rates[i]!=s->clocks.measure[i][3])return 0;
 /* Re-decode fixed immutable file-backed section, never a boot pointer/flag. */
 return g8_slot_copy(s->firmware.firmware,sizeof(s->firmware.firmware),&actual)&&memcmp(&actual,b,sizeof(actual))==0;
}
/* G11_RETAINED_CODEC_BEGIN */
/* SPDX-License-Identifier: MIT */
#include <stddef.h>
#include <stdint.h>
#define GDR_BYTES 64u
#define GDR_PARTS 4u
#define GDR_PART_CHARS 32u
/* Retained CPU state only. No MMIO or private network information. */
struct gdr_state {
 uint32_t phase,error,flags,reads,writes,old_halt,initial_halt,initial_ack;
 uint32_t qualified_snapshots,bus_released,final_ack,restored,poison;
};
uint32_t gdr_crc32(const uint8_t*,size_t);
int gdr_encode(const struct gdr_state*,char [GDR_PARTS][GDR_PART_CHARS+1]);
int gdr_decode(const char [GDR_PARTS][GDR_PART_CHARS+1],struct gdr_state*);

/* SPDX-License-Identifier: MIT */
/* No digits, I or O: in particular, HALT0 and HALT8 encode as A and J. */
static const char alphabet[]="ABCDEFGHJKLMNPQR";
uint32_t gdr_crc32(const uint8_t*p,size_t n){
 uint32_t crc=~0u;
 for(size_t i=0;i<n;i++){crc^=p[i];for(unsigned j=0;j<8;j++)crc=(crc>>1)^(0xedb88320u&-(crc&1u));}
 return ~crc;
}
static void put(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint32_t get(const uint8_t*p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static unsigned nibble(char c){for(unsigned i=0;i<16;i++)if(c==alphabet[i])return i;return 16;}
int gdr_encode(const struct gdr_state*s,char out[GDR_PARTS][GDR_PART_CHARS+1]){
 if(!s||!out)return 0;
 const uint32_t fields[15]={0x3152475a,1,s->phase,s->error,s->flags,s->reads,s->writes,
  s->old_halt,s->initial_halt,s->initial_ack,s->qualified_snapshots,s->bus_released,s->final_ack,s->restored,s->poison};
 uint8_t raw[GDR_BYTES];for(unsigned i=0;i<15;i++)put(raw+4*i,fields[i]);put(raw+60,gdr_crc32(raw,60));
 for(unsigned i=0;i<GDR_BYTES;i++){unsigned part=i/16,at=2*(i%16);out[part][at]=alphabet[raw[i]>>4];out[part][at+1]=alphabet[raw[i]&15];}
 for(unsigned i=0;i<GDR_PARTS;i++)out[i][GDR_PART_CHARS]=0;
 return 1;
}
int gdr_decode(const char in[GDR_PARTS][GDR_PART_CHARS+1],struct gdr_state*out){
 if(!in||!out)return 0;
 uint8_t raw[GDR_BYTES];
 for(unsigned part=0;part<GDR_PARTS;part++){
  if(in[part][GDR_PART_CHARS])return 0;
  for(unsigned i=0;i<16;i++){unsigned a=nibble(in[part][2*i]),b=nibble(in[part][2*i+1]);if(a>15||b>15)return 0;raw[16*part+i]=(uint8_t)(a*16+b);}
 }
 if(get(raw)!=0x3152475a||get(raw+4)!=1||get(raw+60)!=gdr_crc32(raw,60))return 0;
 /* Do not modify the destination on malformed, incomplete or mixed records. */
 *out=(struct gdr_state){get(raw+8),get(raw+12),get(raw+16),get(raw+20),get(raw+24),get(raw+28),
  get(raw+32),get(raw+36),get(raw+40),get(raw+44),get(raw+48),get(raw+52),get(raw+56)};
 return 1;
}

static int zg_gdr_capture(const struct g8_record*r,char out[GDR_PARTS][GDR_PART_CHARS+1]){
 const struct gdr_state s={.phase=r->phase,.error=r->error,.flags=r->flags,
  .reads=r->reads,.writes=r->writes,.old_halt=(uint32_t)r->old[23],
  .initial_halt=r->initial_halt,.initial_ack=r->initial_ack,
  .qualified_snapshots=r->bus_observations,.bus_released=r->bus_released,
  .final_ack=r->final_ack,.restored=r->restored,.poison=r->poison};
 return gdr_encode(&s,out);
}
/* G11_RETAINED_CODEC_END */

/* G13_SNAPSHOT_CODEC_BEGIN */
/* SPDX-License-Identifier: GPL-2.0-only */
#include <stdint.h>
#define GSR_REGS 37u
#define GSR_PAGES 8u
#define GSR_PARTS 4u
#define GSR_CHARS 32u
struct gsr_page {uint32_t page,snapshot_crc;uint64_t invalid,values[5];};
int gsr_encode(const uint64_t old[GSR_REGS],uint64_t invalid,char out[GSR_PAGES][GSR_PARTS][GSR_CHARS+1]);
int gsr_decode_page(const char in[GSR_PARTS][GSR_CHARS+1],struct gsr_page*out);
int gsr_assemble(const struct gsr_page in[GSR_PAGES],uint64_t out[GSR_REGS],uint64_t*invalid);

/* SPDX-License-Identifier: GPL-2.0-only */
static uint32_t gsr_crc(const uint8_t*p,unsigned n){uint32_t c=~0u;for(unsigned i=0;i<n;i++){c^=p[i];for(unsigned b=0;b<8;b++)c=(c>>1)^((0u-(c&1u))&0xedb88320u);}return ~c;}
static void gsr_put(uint8_t*p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
static uint32_t gsr_get(const uint8_t*p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint64_t gsr_u64(const uint8_t*p){return gsr_get(p)|(uint64_t)gsr_get(p+4)<<32;}
static unsigned gsr_nibble(char c){const char*a="ABCDEFGHJKLMNPQR";for(unsigned i=0;i<16;i++)if(a[i]==c)return i;return 16;}
static uint32_t gsr_snapshot_crc(const uint64_t old[GSR_REGS]){uint8_t raw[GSR_REGS*8];for(unsigned i=0;i<GSR_REGS;i++){gsr_put(raw+8*i,(uint32_t)old[i]);gsr_put(raw+8*i+4,(uint32_t)(old[i]>>32));}return gsr_crc(raw,sizeof(raw));}
int gsr_encode(const uint64_t old[GSR_REGS],uint64_t invalid,char out[GSR_PAGES][GSR_PARTS][GSR_CHARS+1]){
 if(!old||!out||invalid>>GSR_REGS)return 0;uint32_t crc=gsr_snapshot_crc(old);const char*a="ABCDEFGHJKLMNPQR";
 for(unsigned page=0;page<GSR_PAGES;page++){uint8_t raw[64]={0};gsr_put(raw,0x3153475a);gsr_put(raw+4,page);gsr_put(raw+8,crc);gsr_put(raw+12,(uint32_t)invalid);gsr_put(raw+16,(uint32_t)(invalid>>32));
  for(unsigned j=0;j<5;j++){unsigned i=page*5+j;uint64_t v=i<GSR_REGS?old[i]:0;gsr_put(raw+20+8*j,(uint32_t)v);gsr_put(raw+24+8*j,(uint32_t)(v>>32));}gsr_put(raw+60,gsr_crc(raw,60));
  for(unsigned p=0;p<GSR_PARTS;p++){for(unsigned i=0;i<16;i++){unsigned v=raw[p*16+i];out[page][p][2*i]=a[v>>4];out[page][p][2*i+1]=a[v&15];}out[page][p][GSR_CHARS]=0;}
 }return 1;
}
int gsr_decode_page(const char in[GSR_PARTS][GSR_CHARS+1],struct gsr_page*out){
 if(!in||!out)return 0;uint8_t raw[64];for(unsigned p=0;p<GSR_PARTS;p++){if(in[p][GSR_CHARS])return 0;for(unsigned i=0;i<16;i++){unsigned a=gsr_nibble(in[p][2*i]),b=gsr_nibble(in[p][2*i+1]);if(a>15||b>15)return 0;raw[p*16+i]=(uint8_t)(a*16+b);}}
 if(gsr_get(raw)!=0x3153475a||gsr_get(raw+4)>=GSR_PAGES||gsr_get(raw+60)!=gsr_crc(raw,60))return 0;
 struct gsr_page result={.page=gsr_get(raw+4),.snapshot_crc=gsr_get(raw+8),.invalid=gsr_u64(raw+12)};if(result.invalid>>GSR_REGS)return 0;
 for(unsigned i=0;i<5;i++){result.values[i]=gsr_u64(raw+20+8*i);if(result.page*5+i>=GSR_REGS&&result.values[i])return 0;}*out=result;return 1;
}
int gsr_assemble(const struct gsr_page in[GSR_PAGES],uint64_t out[GSR_REGS],uint64_t*invalid){
 if(!in||!out||!invalid||in[0].invalid>>GSR_REGS)return 0;uint64_t old[GSR_REGS];
 for(unsigned p=0;p<GSR_PAGES;p++){if(in[p].page!=p||in[p].snapshot_crc!=in[0].snapshot_crc||in[p].invalid!=in[0].invalid)return 0;for(unsigned j=0;j<5;j++){unsigned i=p*5+j;if(i<GSR_REGS)old[i]=in[p].values[j];else if(in[p].values[j])return 0;}}
 if(gsr_snapshot_crc(old)!=in[0].snapshot_crc)return 0;for(unsigned i=0;i<GSR_REGS;i++)out[i]=old[i];*invalid=in[0].invalid;return 1;
}

/* G13_SNAPSHOT_CODEC_END */

static int zg_g8_run(struct zg_state*s){
 if(!g8_slot_copy(s->firmware.firmware,sizeof(s->firmware.firmware),&s->firmware.blob)){printf("ZIYI GMU STOP private slot absent/invalid; no firmware stores\n");return 0;}
 struct g8_prereq q={.epoch=zg_ticks(s),.board=s->contract.board,.hlos_cb=s->contract.cb,.hlos_smr=s->contract.smr,.current_el=(uint32_t)zg_el(),.sctlr=zg_sctlr(),.acd_off=1,.qdss_off=1,.lm_bcl_off=1,.hfi_off=1,.inherited_ddr_probe=1};
 for(unsigned i=0;i<4;i++)q.rates[i]=s->clocks.measure[i][3];
 struct g8_io io={s,zg_g8_read,zg_g8_write,zg_ticks,zg_delay,zg_barrier,zg_lease,zg_g8_proof,zg_g8_owned,{s,zg_g8_physical,zg_g8_flush,zg_g8_clean,zg_barrier}};
 printf("ZIYI GMU FW100 begin: ACD/QDSS/LM/BCL/HFI/GX off; inherited DDR bandwidth probe\n");int success=g8_run(&s->firmware,&q,&io);struct g8_record*r=&s->firmware.record;
 printf("ZIYI GMU result%u phase%u flags%x err%u poison%u reads%u writes%u FW%x reset%u drain%u/%u restore%u lease%u retained%u accel0\n",success,r->phase,r->flags,r->error,r->poison,r->reads,r->writes,(unsigned)r->expected[3],!!(r->flags&G8_HELD_RESET),r->drain.client_ack,r->drain.arb_ack,r->restored,r->lease_released,r->retain_allocations);
 printf("ZIYI GMU faults HFI%x AO%x fence%x SMMU%x last%llx/%llx\n",r->last_host,r->last_ao,r->last_fence,r->last_fsr,(unsigned long long)r->last_address,(unsigned long long)r->last_value);
 printf("ZIYI GMU G12 bus observations%u initial HALT%x ACK%x finalACK%x released%u\n",r->bus_observations,r->initial_halt,r->initial_ack,r->final_ack,r->bus_released);
 printf("ZIYI GMU G12 normalized%u cleanup%u rawRestore%u\n",!!(r->flags&G12_BUS_NORMALIZED),!!(r->flags&G12_NORMALIZED_CLEANUP),r->restored);
 /* Retain and repeat the completed transaction summary so concurrent IOKit
  * chatter cannot erase the only camera sample. No additional MMIO here. */
 /* Snapshot the completed CPU record once. No new MMIO, no guard change.
  * Four short lines repeated for optical recovery; CRC rejects mixed sets. */
 char record[GDR_PARTS][GDR_PART_CHARS+1];
 if(zg_gdr_capture(r,record))for(unsigned repeat=0;repeat<5;repeat++){
  for(unsigned part=0;part<GDR_PARTS;part++)printf("ZGDR%u %s\n",part+1,record[part]);
  zg_sleep(2000);
 }
 /* CPU-only snapshot. No additional MMIO, including on a poisoned lease. */
 if(r->flags&G8_SAVED){
  char pages[GSR_PAGES][GSR_PARTS][GSR_CHARS+1];
  if(gsr_encode(r->old,g8_invalid_registers(&s->firmware),pages))for(unsigned repeat=0;repeat<3;repeat++)for(unsigned page=0;page<GSR_PAGES;page++){
   for(unsigned part=0;part<GSR_PARTS;part++)printf("ZGSP%u%u %s\n",page+1,part+1,pages[page][part]);
   zg_sleep(1500);
  }
 }
 printf("ZIYI GMU G13 context selected%u rebased%u cleanup%u rawRestore%u\n",!!(r->flags&G13_CONTEXT_REINIT),!!(r->flags&G13_CONTEXT_REBASED),!!(r->flags&G13_CONTEXT_CLEANUP),r->restored);
 if(r->poison||r->retain_allocations||!r->lease_released)s->clocks.poison=1;return success;
}
static int zg_qualified(void*ctx,struct g7c_record*clocks){
 struct zg_state*s=ctx;if(!zg_adt_check(s,0)){s->clocks.poison=1;return 0;}if(clocks!=&s->clocks||clocks->flags!=127||clocks->poison||clocks->error)return 0;
 if(s->gmu_enabled)return zg_g8_run(s);
 struct g7_prereq q={.epoch=zg_ticks(s),.board=s->contract.board,.hlos_cb=s->contract.cb,.hlos_smr=s->contract.smr,
 .contract_kind=G7_CONTRACT_XNU_SECURE_DT,.handoff_pairs=s->contract.handoff,.mapped_device_io=1,
 .execution_kind=G7_EXEC_KERNEL_ON,.current_el=(uint32_t)zg_el(),.sctlr=zg_sctlr()};
 for(unsigned i=0;i<4;i++)q.rates[i]=clocks->measure[i][3];
 struct g7_buffers buffers={s->copy0,s->copy1,sizeof(s->copy0),sizeof(s->copy1)};
 struct g7_io io={s,zg_read_probe,zg_ticks,zg_lease,zg_owned,zg_barrier};
 printf("ZIYI GPU G7 acquire kernel-MMU-on, no firmware/DMA\n");
 int success=g7_run_acquire(&s->probe,&q,&buffers,&io);
 printf("ZIYI GPU G7 result flags=%x error=%u poison=%u reads=%u TCM=%u CRC=%08x/%08x fw=0 accel=0\n",s->probe.flags,s->probe.error,s->probe.poison,s->probe.reads,s->probe.tcm_equal,s->probe.tcm_crc[0],s->probe.tcm_crc[1]);
 if(s->probe.poison)clocks->poison=1;return success;
}
static void zg_worker(void*arg,wait_result_t result){
 (void)arg;(void)result;bzero(&zg,sizeof(zg));PE_parse_boot_argn("ziyi_gmu",&zg.gmu_enabled,sizeof(zg.gmu_enabled));if(zg.gmu_enabled!=1)zg.gmu_enabled=0;unsigned delay_seconds=60;int pid=0;
 PE_parse_boot_argn("ziyi_gpu_probe_delay",&delay_seconds,sizeof(delay_seconds));if(delay_seconds>300)delay_seconds=300;
 printf("ZIYI GPU probe armed opt-in delay%us then await SpringBoard; fw=0 accel=0\n",delay_seconds);zg_sleep(delay_seconds*1000u);
 for(unsigned n=0;n<60&&!pid;n++){proc_iterate(PROC_ALLPROCLIST|PROC_NOWAITTRANS,zg_springboard,&pid,NULL,NULL);if(!pid)zg_sleep(5000);}
 if(!pid){printf("ZIYI GPU probe STOP SpringBoard absent after bounded wait; no device maps\n");goto done;}
 zg_sleep(5000);printf("ZIYI GPU probe SpringBoard pid%d alive; starting in-system qualification\n",pid);
 if(!zg_kernel_context(zg_el(),zg_sctlr(),zg_frequency())||!zg_adt_check(&zg,1)){
  printf("ZIYI GPU probe ADT STOP status%u property%u mode%u available%u observed%u; no MMIO\n",zg.adt.status,zg.adt.property,zg.adt.mode,zg.adt.available,zg.adt.observed);goto done;
 }
 printf("ZIYI GPU ADT binding mode%u available%u observed%u snapshots%u; board-bound, no fabricated lockdown\n",zg.adt.mode,zg.adt.available,zg.adt.observed,zg.adt.passes);
 if(!zg_adt_check(&zg,0)||!zg_map_all(&zg)){printf("ZIYI GPU probe: mapping failed; no transaction\n");goto unmap;}
 printf("ZIYI GPU probe: XNU EL1/MMU/cache ON; mappedDeviceRO + guarded clockRW pages=%u\n",zg.rw_count);
 zg.clocks.opaque|=G7C_PLATFORM_SCOPE; /* validated exact board-bound ADT snapshot; no claim of Apple lockdown */
 struct g7c_io io={&zg,zg_read_clock,zg_write_clock,zg_delay,zg_ticks,zg_phase,zg_qualified};
 int success=g7c_run(&zg.clocks,&io);
 printf("ZIYI GPU clocks result=%u flags=%x error=%u poison=%u restore=%u rates=%u/%u/%u/%u\n",success,zg.clocks.flags,zg.clocks.error,zg.clocks.poison,zg.clocks.restore,zg.clocks.measure[0][3],zg.clocks.measure[1][3],zg.clocks.measure[2][3],zg.clocks.measure[3][3]);
 printf("ZIYI GPU clock evidence events%u reads%llu phase%u CXhw%08x/%08x GX%08x/%08x\n",zg.clocks.events,zg.clocks.base.reads,zg.clocks.phase,zg.clocks.cx_on_status,zg.clocks.cx_off_status,zg.clocks.gx_before,zg.clocks.gx_after);zg_sleep(500);
 for(unsigned i=0;i<G2_NREGS;i++){const struct g7c_reg*d=&g7c_regs[i];printf("ZIYI GPU ctl%u %llx+%x old%08x want%08x actual%08x\n",i,d->base,d->offset,zg.clocks.old[i],zg.clocks.expected[i],zg.clocks.final[i]);zg_sleep(250);}
 for(unsigned i=0;i<zg.clocks.events&&i<192;i++){const struct g7c_event*e=&zg.clocks.trace[i];printf("ZIYI GPU store%u ctl%u phase%x value%08x\n",i,e->index,e->phase,e->value);zg_sleep(100);}
 if(zg.gmu_enabled){struct g8_record*r=&zg.firmware.record;
  printf("ZIYI GMU seen100%u last%llx value%llx pending%llx/%u=%llx err%u poison%u writes%u\n",!!(r->flags&G8_FW100),r->last_address,r->last_value,r->pending_address,r->pending_width,r->pending_value,r->error,r->poison,r->writes);zg_sleep(500);
  if(r->flags&G8_SAVED)for(unsigned i=0;i<G8_REGS;i++){const struct g8_reg*d=&g8_regs[i];printf("ZIYI GMU saved%u addr%llx width%u old%llx want%llx\n",i,d->address,d->width,r->old[i],r->expected[i]);zg_sleep(150);}
 }
 printf("ZIYI GPU probe done: opt-in FW100 transaction resets/restores; no persistent GPU or acceleration\n");
unmap:if(!zg_adt_check(&zg,0))printf("ZIYI GPU ADT changed after probe; status%u property%u\n",zg.adt.status,zg.adt.property);if(zg.firmware.record.retain_allocations||zg.lease){printf("ZIYI GMU retain owned IO maps/private pages and CX; cleanup incomplete\n");}else zg_unmap_all(&zg);
done:thread_terminate(current_thread());
}
void ziyi_gpu_probe_start(void){
 uint32_t enabled=0;if(!PE_parse_boot_argn("ziyi_gpu_probe",&enabled,sizeof(enabled))||enabled!=1)return;
 uint32_t expected=0;if(!__atomic_compare_exchange_n(&zg_started,&expected,1,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return;
 thread_t worker=THREAD_NULL;kern_return_t kr=kernel_thread_start(zg_worker,NULL,&worker);
 if(kr!=KERN_SUCCESS){printf("ZIYI GPU probe: cannot create worker %d\n",kr);return;}thread_deallocate(worker);
}
