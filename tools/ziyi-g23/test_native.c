/* SPDX-License-Identifier: GPL-2.0-only
 * Actual adapter functions with CPU-only XNU/firmware mocks. No native claim. */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ziyi_g23_startup.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line%u %s\n",(unsigned)__LINE__,#x);abort();}}while(0)
typedef uintptr_t vm_offset_t;
#define VM_WIMG_IO 1
#define VM_PROT_READ 1
#define G19_BELL 0x03d7e650ULL
#define G19_HFI_COMPLETE 8
#define GH_A662_NATIVE_CORE_VERSION 0x30020004
#define GH_LIVE_RELEASE 2
#define GH_LIVE_ACQUIRE 1
struct g19_hfi_health {uint32_t fw_result,hfi_status,reset,helper_phase,helper_error;};
struct zg_state {
 uint32_t gmu_enabled;uint64_t lease_epoch;
 struct {uint32_t stage,error,version,q1_read_stores;}hfi_record;
 struct {uint64_t va;}hfi_memory;
 struct {struct {uint32_t hfi_address;}firmware_plan;}firmware;
};
static struct zg_state zg;
static struct {
 uint8_t db[0x20000];uint32_t mem[GN_HFI_BYTES/4];uint32_t mode,preuser,adt,owner,healthy,bell,bells,maps,unmaps,preconsume;
 uint64_t tick;
} fake;
static void firmware(void);
static void host_bzero(void*p,size_t n){memset(p,0,n);}
static int PE_parse_boot_argn(const char*n,void*out,size_t bytes){CHECK(bytes==4);if(!strcmp(n,"ziyi_gpu_startup")){*(unsigned*)out=fake.mode;return 1;}if(!strcmp(n,"ziyi_gpu_preuser")){*(unsigned*)out=fake.preuser;return 1;}abort();}
static int zg_adt_check(struct zg_state*s,int first){(void)first;return s==&zg&&fake.adt;}
static vm_offset_t io_map(uint64_t pa,size_t bytes,unsigned kind,unsigned prot,bool unmap){CHECK(pa==0x80860000&&bytes==0x20000&&kind==VM_WIMG_IO&&prot==VM_PROT_READ&&unmap);fake.maps++;return (vm_offset_t)fake.db;}
static void ml_io_unmap(vm_offset_t va,size_t bytes){CHECK(va==(vm_offset_t)fake.db&&bytes==0x20000);fake.unmaps++;}
static void zg_barrier(void*ctx){CHECK(ctx==&zg);if(fake.bell){fake.bell=0;fake.bells++;if(!fake.preconsume)firmware();}}
static int zg_hfi_qualified(void*ctx,uint64_t epoch){return ctx==&zg&&fake.owner&&epoch==zg.lease_epoch;}
static int zg_hfi_read32(void*ctx,uint32_t at,uint32_t*v){CHECK(ctx==&zg&&!(at&3)&&at<GN_HFI_BYTES);*v=fake.mem[at/4];return fake.owner;}
static int zg_hfi_write32(void*ctx,uint32_t at,uint32_t v){CHECK(ctx==&zg&&(at==68||at==112||(at>=GN_HFI_QUEUE0&&at<GN_HFI_QUEUE0+4096)));fake.mem[at/4]=v;if(at==68&&fake.preconsume)firmware();return fake.owner;}
static int zg_hfi_order(void*ctx,unsigned order){CHECK(ctx==&zg);return order==GH_LIVE_ACQUIRE||order==GH_LIVE_RELEASE;}
static int zg_hfi_health(void*ctx,struct g19_hfi_health*h){CHECK(ctx==&zg);*h=(struct g19_hfi_health){0x100,1,0,3,0};return fake.healthy;}
static void*zg_g8_pointer(struct zg_state*s,uint64_t address,unsigned width,int write){CHECK(s==&zg&&address==G19_BELL&&width==4&&write==1);return &fake.bell;}
static uint64_t zg_frequency(void){return 19200000;}
static int zg_hfi_now(void*ctx,uint64_t*out){CHECK(ctx==&zg);*out=fake.tick++;return 1;}
static int zg_hfi_delay(void*ctx,uint32_t us){CHECK(ctx==&zg&&us==100);fake.tick+=1920;return 1;}
static void zg_sleep(unsigned ms){CHECK(ms==1000);}
#define bzero host_bzero
#include "ziyi_g23_native.inc"
static void firmware(void){
 unsigned i=g23.record.current;CHECK(i<5);const struct gs_packet*p=&g23.program.packet[i];unsigned aligned=(p->count+3)&~3u,wr=fake.mem[68/4],start=(wr+1024-aligned)%1024;
 for(unsigned n=0;n<aligned;n++)CHECK(fake.mem[GN_HFI_QUEUE0/4+(start+n)%1024]==(n<p->count?p->words[n]:0xfafafafa));
 fake.mem[64/4]=wr;unsigned at=fake.mem[116/4];fake.mem[(GN_HFI_QUEUE0+4096)/4+at]=0x10301;fake.mem[(GN_HFI_QUEUE0+4096)/4+(at+1)%1024]=p->words[0];fake.mem[(GN_HFI_QUEUE0+4096)/4+(at+2)%1024]=0;fake.mem[116/4]=(at+4)%1024;
}
static uint8_t projection[2248];
static void setup(void){
 memset(&fake,0,sizeof(fake));memcpy(fake.db,projection,sizeof(projection));memcpy(fake.db,g23_expected_header,144);
 fake.mode=fake.preuser=fake.adt=fake.owner=fake.healthy=1;fake.tick=100;zg=(struct zg_state){0};zg.gmu_enabled=2;zg.lease_epoch=0x11223344;
 zg.hfi_record.stage=8;zg.hfi_record.version=0x30020004;zg.hfi_record.q1_read_stores=1;zg.hfi_memory.va=(uint64_t)(uintptr_t)fake.mem;zg.firmware.firmware_plan.hfi_address=0x60006000;
 CHECK(gn_hfi_init(fake.mem,sizeof(fake.mem),0x60006000)==0);fake.mem[64/4]=fake.mem[68/4]=fake.mem[112/4]=fake.mem[116/4]=4;
}
static void pending(void){
 setup();CHECK(ziyi_gpu_startup_prepare(&zg)==1);g23.record.epoch=zg.lease_epoch;CHECK(gs_prepare(&g23.program,&g23.resources,zg.lease_epoch,0x60006000,2136718)==0);
 g23.record.state=GS_RUNNING;g23.record.current=0;g23.record.calls=1;g23.record.acks=0;
 struct gs_step*s=&g23.record.step[0];s->header=g23.program.packet[0].words[0];s->crc=gs_crc((const uint8_t*)g23.program.packet[0].words,59*4);s->stores=61;s->q0_write=64;fake.mem[68/4]=64;
 for(unsigned i=0;i<60;i++)fake.mem[GN_HFI_QUEUE0/4+4+i]=i<59?g23.program.packet[0].words[i]:0xfafafafa;
}
int main(int argc,char**argv){
 CHECK(argc==2);FILE*f=fopen(argv[1],"rb");CHECK(f&&fread(projection,1,sizeof(projection),f)==sizeof(projection));fclose(f);
 for(unsigned race=0;race<2;race++){
  setup();fake.preconsume=race;CHECK(ziyi_gpu_startup_prepare(&zg)==1&&g23.prepared&&g23.record.read_words==65536&&fake.maps==1&&fake.unmaps==1);
  CHECK(ziyi_gpu_startup_continue(&zg)==1&&g23.record.state==GS_COMPLETE&&g23.record.calls==5&&g23.record.acks==5&&fake.bells==5);
  CHECK(g23.record.health[0]==0x100&&g23.record.health[1]==1);
 }
 for(unsigned test=0;test<15;test++){
  pending();
  switch(test){
   case 0:g23.mode=0;break;case 1:g23.prepared=0;break;case 2:g23.record.state=GS_COMPLETE;break;
   case 3:g23.record.current=1;break;case 4:g23.record.calls=2;break;case 5:g23.record.acks=1;break;
   case 6:zg.hfi_record.error=1;break;case 7:zg.hfi_record.version^=1;break;case 8:zg.hfi_record.q1_read_stores=0;break;
   case 9:g23.record.step[0].stores--;break;case 10:g23.record.step[0].crc^=1;break;
   case 11:fake.mem[68/4]^=4;break;case 12:fake.mem[64/4]=12;break;
   case 13:fake.mem[GN_HFI_QUEUE0/4+5]^=1;break;case 14:fake.healthy=0;break;
  }
  CHECK(!zg_g23_bell(&zg,0,&g23.program.packet[0])&&fake.bells==0&&fake.bell==0);
 }
 for(unsigned test=0;test<5;test++){
  setup();if(test==0)fake.preuser=0;if(test==1)zg.gmu_enabled=1;if(test==2)fake.adt=0;if(test==3)fake.db[4]^=1;if(test==4)fake.db[144]^=1;
  CHECK(ziyi_gpu_startup_prepare(&zg)==0&&!g23.prepared&&ziyi_gpu_startup_continue(&zg)==0&&fake.bells==0);
 }
 setup();fake.mode=0;CHECK(ziyi_gpu_startup_prepare(&zg)==0&&fake.maps==0);
 puts("G23_ACTUAL_NATIVE_ADAPTER_HOST_PASS successes2 preDoorbellRace1 guardRefusals15 resourceModeRefusals6 native0");return 0;
}
