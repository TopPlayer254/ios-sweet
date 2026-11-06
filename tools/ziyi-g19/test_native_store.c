#include "ziyi_hfi_diagnostic.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define G19_INIT 0x03d7e618ULL
#define G19_BELL 0x03d7e650ULL
struct zg_state {struct g19_hfi_record hfi_record;struct {uint64_t va;} hfi_memory;};
static struct zg_state state;
static uint32_t shared[GN_HFI_BYTES/4],written,init;
static unsigned barriers,pointers,loads,checks;
static int adt=1,healthy=1;
static int zg_adt_check(struct zg_state*s,int first){assert(s==&state&&!first);return adt;}
static int zg_hfi_health(void*p,struct g19_hfi_health*h){assert(p==&state);memset(h,0,sizeof(*h));return healthy;}
static uint64_t zg_g8_load(struct zg_state*s,uint64_t a,unsigned w,int*ok){assert(s==&state&&a==G19_INIT&&w==4);loads++;*ok=1;return init;}
static void *zg_g8_pointer(struct zg_state*s,uint64_t a,unsigned w,int write){assert(s==&state&&(a==G19_INIT||a==G19_BELL)&&w==4&&write);pointers++;return &written;}
static void zg_barrier(struct zg_state*s){assert(s==&state);barriers++;}
#include "native_store_source.inc"
static void setup(int bell){
 memset(&state,0,sizeof(state));memset(shared,0,sizeof(shared));written=barriers=pointers=loads=init=0;adt=healthy=1;
 state.hfi_memory.va=(uintptr_t)shared;state.hfi_record.stage=bell?G19_HFI_PUBLISH:G19_HFI_INITIALIZE;
 state.hfi_record.start_calls=1;state.hfi_record.doorbells=1;state.hfi_record.q0_write_stores=1;state.hfi_record.stores=5;
 shared[68/4]=4;shared[GN_HFI_QUEUE0/4]=0x100201;shared[GN_HFI_QUEUE0/4+1]=0x20000000;
}
int main(void){
 for(unsigned bell=0;bell<2;bell++){
  setup(bell);assert(zg_hfi_store(&state,bell?G19_BELL:G19_INIT));assert(written==1&&pointers==1&&barriers==2&&loads==!bell);checks++;
  for(unsigned bad=0;bad<(bell?10u:5u);bad++){
   setup(bell);uint64_t address=bell?G19_BELL:G19_INIT;
   if(bad==0)address=G19_BELL+4;
   if(bad==1)adt=0;
   if(bad==2)healthy=0;
   if(bad==3)state.hfi_record.stage=G19_HFI_ACK;
   if(!bell&&bad==4)init=2;
   if(bell&&bad==4)state.hfi_record.doorbells=2;
   if(bell&&bad==5)state.hfi_record.q0_write_stores=0;
   if(bell&&bad==6)state.hfi_record.stores=6;
   if(bell&&bad==7)shared[68/4]=8;
   if(bell&&bad==8)shared[GN_HFI_QUEUE0/4]^=0x100000;
   if(bell&&bad==9)shared[GN_HFI_QUEUE0/4+1]=0x10000000;
   assert(!zg_hfi_store(&state,address)&&!pointers&&!written&&!barriers);checks++;
  }
 }
 printf("G19_ACTUAL_NATIVE_STORE_GUARD_HOST_PASS checks=%u exactBody=1 noWriteOnlyRead=1 hardware=modeled\n",checks);return 0;
}
