/* SPDX-License-Identifier: GPL-2.0-only */
/* Experimental XNU integration of the guarded G6/G7 acquisition engines.
 * No framebuffer/GPU service, firmware execution, DMA or acceleration API.
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
extern vm_offset_t io_map(vm_map_offset_t,vm_size_t,unsigned int,vm_prot_t,bool);
extern void ml_io_unmap(vm_offset_t,vm_size_t);
extern kern_return_t thread_terminate(thread_t);
#define ZG_RW_MAPS 25u
struct zg_mapping {struct zg_region region;struct zg_geometry geometry;vm_offset_t page_va;};
struct zg_state {
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
 return 1;
}
static volatile uint32_t*zg_pointer(struct zg_mapping*m,unsigned count,uint64_t address){
 if(address&3)return 0;
 for(unsigned i=0;i<count;i++){uint64_t offset;if(m[i].page_va&&zg_region_offset(&m[i].region,address,4,&offset))return (volatile uint32_t*)(uintptr_t)(m[i].page_va+m[i].geometry.bias+offset);}
 return 0;
}
static uint32_t zg_read_clock(void*ctx,uint64_t base,uint32_t off,int*ok){
 struct zg_state*s=ctx;*ok=0;if(!g7c_read_allowed(&s->clocks,base,off)||base>UINT64_MAX-off)return 0;
 volatile uint32_t*p=zg_pointer(s->ro,ZG_REGIONS,base+off);if(!p)return 0;
 uint32_t value=*p;zg_barrier(s);*ok=1;return value;
}
static void zg_write_clock(void*ctx,uint64_t base,uint32_t off,uint32_t value,int*ok){
 struct zg_state*s=ctx;*ok=0;if(!g7c_write_allowed(base,off)||base>UINT64_MAX-off)return;
 volatile uint32_t*read=zg_pointer(s->ro,ZG_REGIONS,base+off),*write=zg_pointer(s->rw,s->rw_count,base+off);if(!read||!write)return;
 uint32_t now=*read;zg_barrier(s);for(unsigned i=0;i<G2_NREGS;i++)if(g7c_regs[i].base==base&&g7c_regs[i].offset==off)s->clocks.final[i]=now;
 if(!g7c_store_guard(&s->clocks,base,off,value,now))return;
 *write=value;zg_barrier(s);*ok=1;
}
static uint32_t zg_read_probe(void*ctx,uint64_t address,int*ok){
 struct zg_state*s=ctx;*ok=0;if(!g7_read_allowed(&s->probe,address))return 0;
 volatile uint32_t*p=zg_pointer(s->ro,ZG_REGIONS,address);if(!p)return 0;uint32_t value=*p;zg_barrier(s);*ok=1;return value;
}
static int zg_lease(void*ctx,uint64_t epoch,int acquire){
 struct zg_state*s=ctx;if(!epoch)return 0;
 if(acquire){uint32_t expected=0;if(!__atomic_compare_exchange_n(&s->lease,&expected,1,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 0;s->lease_epoch=epoch;return 1;}
 if(__atomic_load_n(&s->lease,__ATOMIC_ACQUIRE)!=1||s->lease_epoch!=epoch)return 0;s->lease_epoch=0;__atomic_store_n(&s->lease,0,__ATOMIC_RELEASE);return 1;
}
static int zg_owned(void*ctx,const void*memory,size_t bytes){struct zg_state*s=ctx;return bytes==G7_TCM_BYTES&&(memory==s->copy0||memory==s->copy1);}
static void zg_phase(void*ctx,uint32_t phase){(void)ctx;printf("ZIYI GPU clock phase %u\n",phase);}
static int zg_qualified(void*ctx,struct g7c_record*clocks){
 struct zg_state*s=ctx;if(clocks!=&s->clocks||clocks->flags!=127||clocks->poison||clocks->error)return 0;
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
 (void)arg;(void)result;bzero(&zg,sizeof(zg));DTEntry entry=0;unsigned delay_seconds=60;int pid=0;
 PE_parse_boot_argn("ziyi_gpu_probe_delay",&delay_seconds,sizeof(delay_seconds));if(delay_seconds>300)delay_seconds=300;
 printf("ZIYI GPU probe armed opt-in delay%us then await SpringBoard; fw=0 accel=0\n",delay_seconds);zg_sleep(delay_seconds*1000u);
 for(unsigned n=0;n<60&&!pid;n++){proc_iterate(PROC_ALLPROCLIST|PROC_NOWAITTRANS,zg_springboard,&pid,NULL,NULL);if(!pid)zg_sleep(5000);}
 if(!pid){printf("ZIYI GPU probe STOP SpringBoard absent after bounded wait; no device maps\n");goto done;}
 zg_sleep(5000);printf("ZIYI GPU probe SpringBoard pid%d alive; starting in-system qualification\n",pid);
 if(!SecureDTIsLockedDown()||!zg_kernel_context(zg_el(),zg_sctlr(),zg_frequency())||SecureDTLookupEntry(NULL,"/arm-io/ziyi-gpu-probe",&entry)!=kSuccess||!zg_contract_read(&zg.contract,zg_get,(void*)entry)){
  printf("ZIYI GPU probe: kernel/SecureDT contract rejected; no MMIO\n");goto done;
 }
 if(!zg_map_all(&zg)){printf("ZIYI GPU probe: mapping failed; no transaction\n");goto unmap;}
 printf("ZIYI GPU probe: XNU EL1/MMU/cache ON; mappedDeviceRO + guarded clockRW pages=%u\n",zg.rw_count);
 zg.clocks.opaque|=G7C_PLATFORM_SCOPE; /* validated locked SecureDT kernel contract */
 struct g7c_io io={&zg,zg_read_clock,zg_write_clock,zg_delay,zg_ticks,zg_phase,zg_qualified};
 int success=g7c_run(&zg.clocks,&io);
 printf("ZIYI GPU clocks result=%u flags=%x error=%u poison=%u restore=%u rates=%u/%u/%u/%u\n",success,zg.clocks.flags,zg.clocks.error,zg.clocks.poison,zg.clocks.restore,zg.clocks.measure[0][3],zg.clocks.measure[1][3],zg.clocks.measure[2][3],zg.clocks.measure[3][3]);
 printf("ZIYI GPU probe done: acquisition only, firmware=0 DMA=0 acceleration=0\n");
unmap:zg_unmap_all(&zg);
done:thread_terminate(current_thread());
}
void ziyi_gpu_probe_start(void){
 uint32_t enabled=0;if(!PE_parse_boot_argn("ziyi_gpu_probe",&enabled,sizeof(enabled))||enabled!=1)return;
 uint32_t expected=0;if(!__atomic_compare_exchange_n(&zg_started,&expected,1,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return;
 thread_t worker=THREAD_NULL;kern_return_t kr=kernel_thread_start(zg_worker,NULL,&worker);
 if(kr!=KERN_SUCCESS){printf("ZIYI GPU probe: cannot create worker %d\n",kr);return;}thread_deallocate(worker);
}
