/* SPDX-License-Identifier: MIT */
#include "ziyi_wifi_capsule.h"
#include "ziyi_wifi_store.inc"
#if defined(__APPLE__)
#define ATTR __attribute__((section("__DATA_CONST,__ziyi_wifi"),used,aligned(16384)))
#else
#define ATTR __attribute__((used,aligned(16384)))
#endif
/* CI placeholder contains no vendor binary/digest and cannot validate. */
static const uint8_t zwf_slot[ZWF_CAPACITY] ATTR={
 'Z','I','Y','I','_','W','I','F','I','_','S','L','O','T','1',0,
 [16]=1,[22]=192,[25]=1,[28]=73,[29]=89,[30]=73,[31]=90,[36]=160,[41]=1
};
static struct zwf_view zwf_kernel_view;
static unsigned zwf_state;
const uint8_t *ziyi_wifi_image_file(const char*dir,const char*name,uint64_t*size){
 if(size)*size=0;if(!size||!dir||dir[0]!='f'||dir[1]!='w'||dir[2])return 0;
 unsigned state=__atomic_load_n(&zwf_state,__ATOMIC_ACQUIRE);
 if(!state){unsigned wanted=0;if(__atomic_compare_exchange_n(&zwf_state,&wanted,1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)){
   state=zwf_validate(zwf_slot,sizeof(zwf_slot),&zwf_kernel_view)?2:3;__atomic_store_n(&zwf_state,state,__ATOMIC_RELEASE);
  }else state=wanted;}
 /* A parallel caller may retry; never expose a half-validated directory. */
 if(state!=2)return 0;
 if(name&&!strcmp(name,"network")){uint64_t n=0;const uint8_t*p=zwf_find(&zwf_kernel_view,name,&n);return znp_file(p,n,size);}
 return zwf_find(&zwf_kernel_view,name,size);
}
const uint8_t *redwood_image_file(const char*dir,const char*name,uint64_t*size,int*disk_out){
 const uint8_t*p=ziyi_wifi_image_file(dir,name,size);if(disk_out)*disk_out=-1;return p;
}
const uint8_t *hakios_image_file(const char*dir,const char*name,uint64_t*size,int*disk_out){return redwood_image_file(dir,name,size,disk_out);}
