/* CPU-only stand-ins for XNU/kernel hardware surfaces. Never a native claim. */
#include "ziyi_g24_engine.h"
#include "ziyi_g23_startup.h"
#include "ziyi_g19_xnu_owned_pages.h"
#include "ziyi_g7_sha256.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
typedef uint64_t vm_offset_t;typedef unsigned vm_prot_t;
#define PAGE_SIZE 16384u
#define VM_PROT_READ 1u
#define VM_PROT_WRITE 2u
#define VM_WIMG_IO 1u
#define VM_WIMG_WCOMB 2u
struct zg_region{uint64_t base,bytes;};struct zg_geometry{uint64_t page_base,page_span,bias;};
struct zg_mapping{struct zg_region region;struct zg_geometry geometry;vm_offset_t page_va;};
struct zg_state{uint64_t lease_epoch;uint32_t lease,gmu_enabled;struct hop hfi_memory;struct hop_xnu hfi_owner;};
static struct zg_state zg;static struct{struct gs_record record;}g23;
typedef struct{uint64_t v_baseAddr,v_width,v_height,v_rowBytes;} PE_Video;
static uint8_t core[0x40000],gmu[0x38000],smmu[0x20000],reserved[16384],framebuffer[1080*2400*4];
static uint8_t *allocation,*raw_allocation;static uint8_t capsule[65536];static uint64_t ticks;
static unsigned boot_cp=1,adt=1,failed_map,validate_fail,metadata_interrupt,smc_calls;
static void zg_barrier(void*c){(void)c;}
static uint64_t zg_ticks(void*c){(void)c;return ticks+=100;}
static uint64_t zg_frequency(void){return 19200000;}
static void zg_delay(void*c,uint32_t us){(void)c;assert(us<=100);ticks+=(uint64_t)us*192/10;}
static void zg_sleep(unsigned ms){(void)ms;abort();}
static int zg_adt_check(struct zg_state*s,int first){(void)first;return s==&zg&&adt;}
static int PE_parse_boot_argn(const char*n,void*out,int bytes){assert(bytes==4);*(unsigned*)out=!strcmp(n,"ziyi_gpu_cp")?boot_cp:1;return 1;}
static void PE_current_console(PE_Video*v){*v=(PE_Video){0xb8000000,1080,2400,4320};}
static int zg_map_geometry(uint64_t base,uint64_t bytes,uint64_t page,struct zg_geometry*out){if(!base||!bytes||page!=16384||base>UINT64_MAX-bytes)return 0;out->page_base=base&~16383ULL;out->bias=base-out->page_base;out->page_span=(out->bias+bytes+16383)&~16383ULL;return 1;}
static int zg_region_offset(const struct zg_region*r,uint64_t address,uint64_t n,uint64_t*out){if(address<r->base||address-r->base>=r->bytes||n>r->bytes-(address-r->base))return 0;*out=address-r->base;return 1;}
static uint8_t*physical(uint64_t pa){if(pa>=0x3d00000&&pa<0x3d40000)return core+(pa-0x3d00000);if(pa>=0x3d68000&&pa<0x3da0000)return gmu+(pa-0x3d68000);if(pa>=0x3da0000&&pa<0x3dc0000)return smmu+(pa-0x3da0000);if(pa>=0x8bb18000&&pa<0x8bb1c000)return reserved+(pa-0x8bb18000);if(pa>=0xb8000000&&pa<0xb89e3400)return framebuffer+(pa-0xb8000000);return NULL;}
static vm_offset_t io_map(uint64_t base,uint64_t bytes,unsigned cache,vm_prot_t prot,bool unmappable){assert(unmappable&&prot==3&&bytes);if(failed_map)return 0;assert(cache==VM_WIMG_IO||cache==VM_WIMG_WCOMB);uint8_t*p=physical(base);assert(p&&physical(base+bytes-1));return (uintptr_t)p;}
static void ml_io_unmap(vm_offset_t va,uint64_t bytes){assert(va&&(bytes%16384)==0);}
static int zg_map_one(struct zg_mapping*m,struct zg_region r,vm_prot_t p){struct zg_geometry g;if(!zg_map_geometry(r.base,r.bytes,16384,&g))return 0;vm_offset_t va=io_map(g.page_base,g.page_span,VM_WIMG_IO,p,true);if(!va)return 0;*m=(struct zg_mapping){r,g,va};return 1;}
static int zg_hfi_evidence(void*ctx,uint64_t epoch,uint32_t*facts){if(ctx!=&zg||epoch!=zg.lease_epoch)return 0;*facts=HOP_RECLAIM;return 1;}
int hop_xnu_prepare(struct hop*h,struct hop_xnu*x,uint64_t epoch,uint32_t n){assert(h&&x&&n==8&&!h->state&&epoch==zg.lease_epoch);uint32_t facts;assert(x->owner_evidence(x->owner_ctx,epoch,&facts)&&facts==HOP_RECLAIM);raw_allocation=calloc(1,8*16384+16384);assert(raw_allocation);allocation=(uint8_t*)(((uintptr_t)raw_allocation+16383)&~(uintptr_t)16383);h->va=x->va=(uintptr_t)allocation;h->epoch=x->epoch=epoch;h->bytes=x->bytes=n*16384;h->count=n;h->state=HOP_READY;for(unsigned i=0;i<n;i++)h->page[i].pa=0x900100000ULL+i*16384;return 1;}
int hop_xnu_validate(struct hop*h,struct hop_xnu*x){return !validate_fail&&h&&x&&h->epoch==zg.lease_epoch&&(h->state==HOP_READY||h->state==HOP_LIVE);}
int hop_xnu_mark_live(struct hop*h,struct hop_xnu*x){if(!hop_xnu_validate(h,x))return 0;h->state=HOP_LIVE;return 1;}
static uint64_t kvtophys(vm_offset_t va){assert(va>=(uintptr_t)allocation&&va<(uintptr_t)allocation+8*16384);return 0x900100000ULL+va-(uintptr_t)allocation;}
#if defined(G24_PUBLIC_FIXTURE)
static uint8_t g24_own_sha[3][32];
#else
#include "ziyi_g24_pins.inc"
#endif
static int g24_test_slot_view(struct g24_firmware*v){return g24_capsule_view(capsule,sizeof(capsule),g24_own_sha,v);}
#include "hwcg-block.inc"
static unsigned g27_bad_zero,g27_bad_roq;
static unsigned g26_clock_cursor,g26_clock_enabled,g26_defaults,g26_unstable,g26_bad_receipt,g26_clock_reads,g26_roq_reads;
static void g24_test_after_store(uint64_t,uint32_t);static void g24_test_smc(uint64_t[8]);
#define G24_HOST_TEST 1
#define g24_slot_view g24_test_slot_view
#include "ziyi_g24_native.inc"
#undef g24_slot_view
struct model{uint32_t ctl[G24_CTL_COUNT];unsigned bad_fence,bad_token,bad_pixel,bad_guard;};static struct model model;
static unsigned odd(uint32_t n){unsigned p=1;while(n){p^=n&1;n>>=1;}return p;}
static void consume(struct model*x,uint32_t end){
 uint32_t reg[0xc000]={0};uint32_t at=x->ctl[G24_RPTR];
 while(at<end){uint32_t h=g24n.w.ring[at++];unsigned type=h>>28;
  if(type==4){unsigned count=h&0x7f,addr=h>>8&0x3ffff;assert(count==1&&at<end&&addr<0xc000);assert((h>>7&1)==odd(count)&&(h>>27&1)==odd(addr));reg[addr]=g24n.w.ring[at++];continue;}
  assert(type==7);unsigned op=h>>16&0x7f,count=h&0x7fff;assert((h>>15&1)==odd(count)&&(h>>23&1)==odd(op)&&count<=end-at);
  if(op==0x48){assert(count==8&&g24n.w.ring[at]==0x2f&&g24n.w.ring[at+1]==3&&g24n.w.ring[at+2]==0x20000000);}
  else if(op==0x66){assert(count==1&&g24n.w.ring[at]==0&&g24n.w.r.pas_authenticated);}
  else if(op==0x3d){assert(count==3&&g24n.w.ring[at]==g24n.w.memory.fence_iova+8&&!g24n.w.ring[at+1]);if(!x->bad_fence)g24n.w.fence[2]=g24n.w.ring[at+2];}
  else if(op==0x46){assert(count==4&&g24n.w.ring[at+2]==0);unsigned offset=g24n.w.ring[at+1]-g24n.w.memory.fence_iova;assert((offset==0||offset==4)&&(g24n.w.ring[at]==0x1d||g24n.w.ring[at]==4));if(!x->bad_token)g24n.w.fence[offset/4]=g24n.w.ring[at+3];}
  else if(op==0x2c){assert(count==1&&g24n.w.ring[at]==3&&reg[0x8c18]==g24n.w.memory.pixel_iova&&reg[0x8c19]==0&&(reg[0x8c1a]<<6)==256);unsigned x1=reg[0x8405]&65535,y1=reg[0x8405]>>16,x2=(reg[0x8406]&65535)+1,y2=(reg[0x8406]>>16)+1;assert(x1<x2&&x2<=64&&y1<y2&&y2<=64);for(unsigned y=y1;y<y2;y++)for(unsigned k=x1;k<x2;k++){unsigned off=y*256+4*k;g24n.w.pixels[off]=(uint8_t)reg[0x8c2e];g24n.w.pixels[off+1]=(uint8_t)reg[0x8c2d];g24n.w.pixels[off+2]=(uint8_t)reg[0x8c2c];g24n.w.pixels[off+3]=(uint8_t)reg[0x8c2f];}}
  else assert(op==0x26||op==0x65);
  at+=count;
 }
 assert(at==end);if(x->bad_pixel&&g24n.w.r.phase==G24_BLIT)g24n.w.pixels[8191]^=1;if(x->bad_guard&&g24n.w.r.phase==G24_BLIT)((volatile uint8_t*)g24n.w.fence)[150]^=1;x->ctl[G24_RPTR]=end;
}

static uint32_t value(unsigned id){uint32_t*p=(uint32_t*)physical(g24_register_address(G24_CTL,id));assert(p);return *p;}
static void set(unsigned id,uint32_t v){uint32_t*p=(uint32_t*)physical(g24_register_address(G24_CTL,id));assert(p);*p=v;}
static void g24_test_after_store(uint64_t address,uint32_t v){for(unsigned i=0;i<G24_CTL_COUNT;i++)model.ctl[i]=value(i);if(address==g24_register_address(G24_CTL,G24_REQUEST)){model.ctl[G24_INFO]|=0x80000000;model.ctl[G24_SPTPRAC]=0;model.ctl[G24_GDSC]=0x80000000;}if(address==g24_register_address(G24_CTL,G24_ACK))model.ctl[G24_INFO]&=~v;if(address==g24_register_address(G24_CTL,G24_WPTR))consume(&model,v);for(unsigned i=0;i<G24_CTL_COUNT;i++)set(i,model.ctl[i]);
 if(g24n.w.r.phase==G24_CONFIG&&g26_clock_cursor<52&&address==g26_clock[g26_clock_cursor].address&&(v&g26_clock[g26_clock_cursor].mask)==g26_clock[g26_clock_cursor].value){
  g26_clock_cursor++;
  if(g26_clock_cursor==52){g26_clock_enabled=1;if(g26_defaults){*(uint32_t*)physical(0x03d02308)=0x00010040;*(uint32_t*)physical(0x03d0230c)=64;}}
 }
 if(address==0x03d02308&&!g26_clock_enabled)*(uint32_t*)physical(address)=0;
 if(address==0x03d00400){*(uint32_t*)physical(address)=v&~0x00040000u;if(g27_bad_zero)*(uint32_t*)physical(address)=1;}
 if(address==0x03d02308&&g27_bad_roq)*(uint32_t*)physical(address)=0;
}
static void g24_test_smc(uint64_t r[8]){smc_calls++;if(r[0]==1){assert(r[1]==130&&r[2]==13&&r[3]==g24n.pages.page[7].pa&&r[6]==0x1234);r[0]=0;r[1]=0;return;}unsigned fn=(unsigned)r[0]&65535;if(fn==0x601){r[0]=0;r[1]=1;return;}assert(r[2]==13);if(fn==0x201&&metadata_interrupt){r[0]=1;r[6]=0x1234;return;}if(fn==0x205)assert(g24n.zap_copied&&g24n.zap_backed);r[0]=0;r[1]=0;}
static void reset(void){if(raw_allocation)free(raw_allocation);raw_allocation=allocation=NULL;memset(&zg,0,sizeof(zg));memset(&g23,0,sizeof(g23));memset(&g24n,0,sizeof(g24n));memset(&model,0,sizeof(model));memset(core,0,sizeof(core));memset(gmu,0,sizeof(gmu));memset(smmu,0,sizeof(smmu));memset(framebuffer,0,sizeof(framebuffer));for(unsigned i=0;i<16384;i++)reserved[i]=(uint8_t)(i*13+5);zg.lease_epoch=0x123456789abcdefULL;zg.lease=1;zg.gmu_enabled=2;zg.hfi_memory.state=HOP_LIVE;zg.hfi_memory.epoch=zg.hfi_owner.epoch=zg.lease_epoch;zg.hfi_memory.page[0].pa=0x900000000ULL;boot_cp=adt=1;failed_map=validate_fail=smc_calls=metadata_interrupt=0;ticks=1000;set(G24_S2CR,0x20000);set(G24_FSR,0x400);}
static void live(void){g23.record.state=GS_COMPLETE;g23.record.acks=5;g23.record.epoch=zg.lease_epoch;set(G24_GMU_SMR,0x84000005);set(G24_GMU_S2CR,0x302);set(G24_GMU_SCTLR,0xc00027);set(G24_FW,0x100);set(G24_HFI,1);for(unsigned i=2;i<6;i++)*(uint32_t*)physical(0x3da0c00+4*i)=0x20000;}
static struct g24_io io(void){return(struct g24_io){&zg,g24n_qualified,g24n_read,g24n_write,g24n_order,g24n_delay,g24n_now,g24n_zap_copy,g24n_present,{&zg,g24n_pas_qualified,g24n_smc}};}
int legacy_native_main(int argc,char**argv){assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f&&fread(capsule,sizeof(capsule),1,f)==1);assert(!fclose(f));
#if defined(G24_PUBLIC_FIXTURE)
 for(unsigned i=0;i<3;i++){unsigned at=i==0?256:i==1?43328:46896,n=i==0?43068:i==1?3564:1976;g7_sha256(capsule+at,n,g24_own_sha[i]);}
#endif

 reset();boot_cp=0;assert(ziyi_gpu_cp_prepare(&zg)&&!g24n.mode&&!allocation);
 for(unsigned interrupted=0;interrupted<2;interrupted++){reset();assert(ziyi_gpu_cp_prepare(&zg)&&g24n.pages.count==8&&g24n.pages.state==HOP_LIVE&&g24n.metadata_verified);live();metadata_interrupt=interrupted;uint8_t before[16384];memcpy(before,reserved,sizeof(before));struct g24_io o=io();int ok=g24_run(&g24n.w,&o);if(!ok){fprintf(stderr,"NATIVE_MODEL_REFUSED phase%u err%u pending%u/%u/%x writes%u route%u cursor%u cp%u rptr%u wptr%u pas%u/%u\n",g24n.w.r.phase,g24n.w.r.error,g24n.w.r.pending_space,g24n.w.r.pending_index,g24n.w.r.pending_value,g24n.w.r.writes,g24n.w.r.route_live,g24n.w.r.smmu_cursor,g24n.w.r.cp_cursor,g24n.w.r.rptr,g24n.w.r.wptr,g24n.w.pas.state,g24n.w.pas.error);}assert(ok);assert(g24n.w.r.phase==G24_DONE&&g24n.w.r.verified_pixels==4096&&g24n.w.r.presented&&g24n.w.r.proofs==127&&g24n.zap_copied&&smc_calls==7+interrupted);assert(!memcmp(before,reserved,8192)&&!memcmp(before+8192+1976,reserved+8192+1976,16384-8192-1976));assert(!memcmp(reserved+8192,g24n.w.fw.code,1976));for(unsigned y=0;y<2400;y++)for(unsigned x=0;x<1080;x++){uint32_t v=*(uint32_t*)(framebuffer+4*(y*1080+x));if(y>=512&&y<1024&&x>=284&&x<796)assert(v==(y<768?0xffff8000u:0xff0080ffu));else assert(v==0);}assert(!g24n_write(&zg,G24_CTL,G24_WPTR,104));}
 for(unsigned bad=0;bad<7;bad++){reset();assert(ziyi_gpu_cp_prepare(&zg));live();if(bad==0)adt=0;if(bad==1)validate_fail=1;if(bad==2)g23.record.epoch++;if(bad==3)set(G24_GMU_SMR,0x84000004);if(bad==4)((uint64_t*)allocation)[0]^=1;if(bad==5)g24n.w.r.epoch++;if(bad==6)model.bad_pixel=1;struct g24_io o=io();assert(!g24_run(&g24n.w,&o)&&!g24n.w.r.presented);}
 reset();assert(ziyi_gpu_cp_prepare(&zg));live();g24n.w.r.phase=G24_POWER;g24n.w.r.pending_space=G24_CTL;g24n.w.r.pending_index=G24_REQUEST;g24n.w.r.pending_value=0x40000000;assert(!g24n_write(&zg,G24_CTL,G24_REQUEST,0x80000000));assert(!g24n_write(&zg,G24_CTL_COUNT,0,0));assert(!g24n_write(&zg,G24_PROFILE,0,1));assert(!g24n_zap_copy(&zg));
 /* Reject a mutually corrupted RAM table and its BSS mirror. */
 for(unsigned leaf=0;leaf<24;leaf++){reset();assert(ziyi_gpu_cp_prepare(&zg));live();unsigned at=((GC_IOVA>>12)&511)+leaf;uint64_t mask=leaf&1?0x40:0x1000;g24n.w.memory.tables[2][at]^=mask;((uint64_t*)allocation)[1024+at]^=mask;assert(!g24n_tables());struct g24_io o=io();assert(!g24_run(&g24n.w,&o)&&!g24n.w.r.writes);}
 /* Continuation tokens do not authorize changed original arguments. */
 reset();assert(ziyi_gpu_cp_prepare(&zg));live();g24n.w.r.phase=G24_ZAP;g24n.w.r.cp_started=1;g24n.w.pas.state=G24P_PROBED;g24n.w.pas.last[0]=1;g24n.w.pas.last[6]=0x1234;
 for(unsigned bad=0;bad<7;bad++){uint64_t r[8]={1,130,13,g24n.pages.page[7].pa,0,0,0x1234,0};r[bad+1]^=1;unsigned before=smc_calls;assert(!g24n_smc(&zg,r)&&smc_calls==before);}
 printf("G24_ACTUAL_NATIVE_ADAPTER_HOST_PASS fullPass2 interruptedMetadata1 refusalCases42 framebufferGuardPixels%d native0\n",1080*2400);free(raw_allocation);return 0;}

static int g26_read(void*ctx,unsigned space,unsigned id,uint32_t*out){
 int ok=g24n_read(ctx,space,id,out);if(!ok)return 0;
 if(g26_clock_enabled&&g24n.w.r.phase==G24_CONFIG&&space==G24_PROFILE){
  if(g26_bad_receipt&&id==63&&g24n.w.r.profile_cursor==63)g24n.w.r.pending_index=62;
  if(g24n.w.r.profile_cursor==64&&id>=64&&g26_clock_reads<174){assert(id==64+g26_clock_reads%87);g26_clock_reads++;
   if(g24_register_address(space,id)==0x03d02308){g26_roq_reads++;assert(*out==0x00010040);}
   if(g26_unstable&&id==64&&g26_clock_reads>87)*out^=1;
  }
 }
 return 1;
}
static void g26_reset(void){reset();g27_bad_zero=g27_bad_roq=0;g26_clock_cursor=g26_clock_enabled=g26_unstable=g26_bad_receipt=g26_clock_reads=g26_roq_reads=0;g26_defaults=1;assert(ziyi_gpu_cp_prepare(&zg));live();}
int main(int argc,char**argv){assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f&&fread(capsule,sizeof(capsule),1,f)==1&&!fclose(f));
 for(unsigned i=0;i<3;i++){unsigned at=i==0?256:i==1?43328:46896,n=i==0?43068:i==1?3564:1976;g7_sha256(capsule+at,n,g24_own_sha[i]);}
 g26_reset();struct g24_io o=io();o.read=g26_read;
#if defined(G27_OLD_CONTROL)
 assert(!g24_run(&g24n.w,&o));assert(g24n.w.r.error==G24_E_CONFIG&&g24n.w.r.failure_phase==G24_CONFIG&&g24n.w.r.profile_cursor==32&&g24n.w.r.profile_count==151&&g24n.w.r.reads==455&&g24n.w.r.writes==51&&g24n.w.r.last_value==0xf00&&!g26_clock_enabled&&!smc_calls&&!g24n.w.r.cp_started);
 puts("G27_G26_HYSTERESIS_NEGATIVE_PASS reads455 stores51 CONFIG406 profile32 noCP noPAS native0");
#else
 assert(g24_run(&g24n.w,&o));assert(g26_clock_cursor==52&&g26_clock_enabled&&g26_clock_reads==174&&g26_roq_reads==2&&g24n.w.r.phase==G24_DONE&&g24n.w.r.verified_pixels==4096&&g24n.w.r.presented&&g24n.w.r.profile_old[76]==0&&g24n.w.r.profile_old[78]==0);
 g26_reset();g26_unstable=1;o=io();o.read=g26_read;assert(!g24_run(&g24n.w,&o));assert(g24n.w.r.error==G24_E_CONFIG&&g24n.w.r.profile_cursor==64&&g26_clock_reads==88&&!g24n.w.r.cp_started&&!smc_calls);
 g26_reset();g26_bad_receipt=1;o=io();o.read=g26_read;assert(!g24_run(&g24n.w,&o));assert(g24n.w.r.error==G24_E_OWNER&&!g26_clock_reads&&!g24n.w.r.cp_started&&!smc_calls);
 g26_reset();g27_bad_zero=1;o=io();o.read=g26_read;assert(!g24_run(&g24n.w,&o));assert(g24n.w.r.error==G24_E_CONFIG&&g24n.w.r.profile_cursor==32&&g24n.w.r.pending_value==0&&g24n.w.r.last_value==1&&!g24n.w.r.cp_started&&!smc_calls);
 g26_reset();g27_bad_roq=1;o=io();o.read=g26_read;assert(!g24_run(&g24n.w,&o));assert(g24n.w.r.error==G24_E_CONFIG&&g24n.w.r.profile_cursor==76&&g24n.w.r.pending_value==0x02000140&&g24n.w.r.last_value==0&&g26_clock_cursor==52&&!g24n.w.r.cp_started&&!smc_calls);
 puts("G27_ACTUAL_GX_OFF_PASS completeHWCG52 stableReads174 originalValuesRetained2 changingViewRefused badReceiptRefused zeroModeWrongReadRefused ROQZeroReadRefused verifiedPixels4096 native0");
#endif
 free(raw_allocation);return 0;}
