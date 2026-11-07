/* G23 legacy-HFI startup; only after successful native G22 version ACK.
 * Additional hardware stores are the SAME exact HFI doorbell address/value.
 * No GX OOB, core registers, CP, ZAP, rendering or user client in this stage. */
#include "ziyi_g23_expected.h"
static struct {
 uint32_t mode,prepared;
 struct gr_resources resources;struct gs_program program;struct gs_record record;
 uint8_t snapshot[2][0x20000] __attribute__((aligned(16)));
} g23;
__attribute__((noinline)) int ziyi_gpu_startup_prepare(void*context){
 struct zg_state*s=context;unsigned enabled=0,preuser=0;
 PE_parse_boot_argn("ziyi_gpu_startup",&enabled,sizeof(enabled));if(enabled!=1)return 0;
 bzero(&g23,sizeof(g23));g23.mode=1;struct gs_record*r=&g23.record;
 PE_parse_boot_argn("ziyi_gpu_preuser",&preuser,sizeof(preuser));
 if(s!=&zg||preuser!=1||s->gmu_enabled!=2||!zg_adt_check(s,0)){r->state=GS_POISON;r->error=GS_ARGUMENT;return 0;}
 /* Own DT no-map CMDDB reservation, source-bound at 0x80860000+0x20000.
  * Outside XNU's managed arena. READ-only Device mapping; no device borrower
  * receives it. Both complete snapshots are private CPU buffers. */
 vm_offset_t va=io_map(0x80860000ULL,0x20000,VM_WIMG_IO,VM_PROT_READ,true);
 if(!va){r->state=GS_POISON;r->error=GS_IO;return 0;}
 for(unsigned pass=0;pass<2;pass++){
  if(!zg_adt_check(s,0)){r->state=GS_POISON;r->error=GS_OWNER;break;}
  zg_barrier(s);for(unsigned off=0;off<0x20000;off+=4){uint32_t v=*(volatile uint32_t*)(uintptr_t)(va+off);memcpy(&g23.snapshot[pass][off],&v,4);r->read_words++;}zg_barrier(s);
 }
 ml_io_unmap(va,0x20000);if(r->error)return 0;
 r->header_crc=gs_crc(g23.snapshot[0],144);
 r->header_equal=!memcmp(g23.snapshot[0],g23_expected_header,144)&&!memcmp(g23.snapshot[1],g23_expected_header,144);
 int error=gr_stable(g23.snapshot[0],0x20000,g23.snapshot[1],0x20000,&g23.resources);
 r->found=g23.resources.found;r->entries=g23.resources.entries;r->resource_crc=gs_crc((const uint8_t*)&g23.resources,sizeof(g23.resources));
 r->resources_equal=!memcmp(&g23.resources,&g23_expected_resources,sizeof(g23.resources));
 if(error||!r->header_equal||!r->resources_equal){r->state=GS_POISON;r->error=350+(unsigned)error;printf("ZIYI G23 resource STOP e%u found%x header%u exact%u crc%08x\n",r->error,r->found,r->header_equal,r->resources_equal,r->resource_crc);return 0;}
 r->state=GS_PREPARED;r->peak_kbps=G23_PEAK_KBPS;g23.prepared=1;
 printf("ZIYI G23 own CMDDB stable/exact: ARC3 BCM4, SH0/MC0/ACV/CN0; peak%u; startup pending\n",G23_PEAK_KBPS);return 1;
}
static int zg_g23_qualified(void*ctx,uint64_t epoch){
 struct zg_state*s=ctx;
 return s==&zg&&g23.mode==1&&g23.prepared&&g23.record.header_equal&&g23.record.resources_equal&&
  g23.record.epoch==epoch&&g23.program.epoch==epoch&&
  !memcmp(&g23.program.resources,&g23_expected_resources,sizeof(g23.resources))&&
  s->hfi_record.stage==G19_HFI_COMPLETE&&!s->hfi_record.error&&s->hfi_record.version==GH_A662_NATIVE_CORE_VERSION&&
  s->hfi_record.q1_read_stores==1&&zg_hfi_qualified(s,epoch);
}
static int zg_g23_read(void*ctx,uint32_t at,uint32_t*v){return zg_g23_qualified(ctx,g23.record.epoch)&&zg_hfi_read32(ctx,at,v);}
static int zg_g23_write(void*ctx,uint32_t at,uint32_t v){return zg_g23_qualified(ctx,g23.record.epoch)&&zg_hfi_write32(ctx,at,v);}
static int zg_g23_order(void*ctx,unsigned order){
 return zg_g23_qualified(ctx,g23.record.epoch)&&zg_hfi_order(ctx,order==GS_RELEASE?GH_LIVE_RELEASE:order==GS_ACQUIRE?GH_LIVE_ACQUIRE:0);
}
static int zg_g23_healthy(void*ctx){
 struct g19_hfi_health h={0};int good=zg_hfi_health(ctx,&h);
 g23.record.health[0]=h.fw_result;g23.record.health[1]=h.hfi_status;g23.record.health[2]=h.reset;g23.record.health[3]=h.helper_phase;g23.record.health[4]=h.helper_error;return good;
}
static int zg_g23_bell(void*ctx,unsigned index,const struct gs_packet*packet){
 struct zg_state*s=ctx;struct gs_record*r=&g23.record;
 if(index>=5||r->state!=GS_RUNNING||r->current!=index||r->calls!=index+1||r->acks!=index||packet!=&g23.program.packet[index]||!zg_g23_qualified(s,r->epoch)||!zg_g23_healthy(s)||!zg_adt_check(s,0))return 0;
 const struct gs_step*step=&r->step[index];uint32_t aligned=(packet->count+3)&~3u;
 if(step->header!=packet->words[0]||step->crc!=gs_crc((const uint8_t*)packet->words,4*packet->count)||step->stores!=aligned+1)return 0;
 volatile uint32_t*q=(volatile uint32_t*)(uintptr_t)s->hfi_memory.va;
 uint32_t start=(step->q0_write+1024-aligned)%1024;
 if(q[68/4]!=step->q0_write||(q[64/4]!=start&&q[64/4]!=step->q0_write))return 0;
 for(unsigned i=0;i<aligned;i++)if(q[GN_HFI_QUEUE0/4+(start+i)%1024]!=(i<packet->count?packet->words[i]:0xfafafafa))return 0;
 void*p=zg_g8_pointer(s,G19_BELL,4,1);if(!p)return 0;
 /* Same finite write-only doorbell, once for this exact typed packet. */
 zg_barrier(s);*(volatile uint32_t*)p=1;zg_barrier(s);return 1;
}
__attribute__((noinline)) int ziyi_gpu_startup_continue(void*context){
 struct zg_state*s=context;if(g23.mode!=1)return 0;
 if(s!=&zg||!g23.prepared||g23.record.state!=GS_PREPARED)return 0;
 struct gs_record*r=&g23.record;r->epoch=s->lease_epoch;
 if(gs_prepare(&g23.program,&g23.resources,r->epoch,s->firmware.firmware_plan.hfi_address,G23_PEAK_KBPS)){r->state=GS_POISON;r->error=GS_ARGUMENT;return 0;}
 const struct gs_io io={s,zg_g23_qualified,zg_g23_read,zg_g23_write,zg_g23_order,zg_g23_bell,zg_g23_healthy,zg_hfi_now,zg_hfi_delay};
 int error=gs_run(&g23.program,&io,zg_frequency(),r);
 printf("ZIYI G23 startup result state%u err%d command%u calls%u ACKs%u; GX OOB/core/CP off\n",r->state,error,r->current,r->calls,r->acks);return !error;
}
__attribute__((noinline)) void ziyi_gpu_startup_print(void*context){
 if(context!=&zg||g23.mode!=1)return;uint8_t raw[GS_RECORD_BYTES];
 if(gs_encode(&g23.record,raw)){printf("ZIYI G23 record invalid; no result inferred\n");return;}
 const char*a="ABCDEFGHJKLMNPQR";char lines[48][33];
 for(unsigned i=0;i<GS_RECORD_BYTES;i++){lines[i/16][2*(i%16)]=a[raw[i]>>4];lines[i/16][2*(i%16)+1]=a[raw[i]&15];}
 for(unsigned i=0;i<48;i++)lines[i][32]=0;
 for(unsigned repeat=0;repeat<2;repeat++)for(unsigned page=0;page<12;page++){
  for(unsigned part=0;part<4;part++)printf("ZGST%02u%u %s\n",page+1,part+1,lines[page*4+part]);zg_sleep(1000);
 }
}
