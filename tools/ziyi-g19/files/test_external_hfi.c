#include "ziyi_g8_memory.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static struct gm_xnu_memory m;
static uint8_t storage[GN_HFI_BYTES+16384],*shared;
static unsigned flushes,cleans,barriers;
static uint64_t physical(void*ctx,const void*p){
 (void)ctx;uintptr_t v=(uintptr_t)p;
 if(v>=(uintptr_t)shared&&v-(uintptr_t)shared<GN_HFI_BYTES)return 0x920000000ULL+v-(uintptr_t)shared;
 if(v>=(uintptr_t)m.backing&&v-(uintptr_t)m.backing<sizeof(m.backing))return 0x900000000ULL+2*(v-(uintptr_t)m.backing);
 if(v>=(uintptr_t)m.tables&&v-(uintptr_t)m.tables<sizeof(m.tables))return 0x910000000ULL+v-(uintptr_t)m.tables;
 return 0;
}
static int flush(void*c,void*p,size_t n){(void)c;assert(p==m.backing&&n==0xa000);flushes++;return 1;}
static int clean(void*c,void*p,size_t n){(void)c;assert(n==4096&&p==m.tables[cleans]);cleans++;return 1;}
static void barrier(void*c){(void)c;barriers++;}
static struct gn_plan fw={.region_count=3,.backing_bytes=0xa000,.hfi_address=0x60004000,.regions={{0x4000,0x2000,GN_CACHE},{0x60000000,0x4000,GN_LOG},{0x60004000,0x4000,GN_HFI}}};
int main(void){
 shared=(uint8_t*)(((uintptr_t)storage+16383u)&~(uintptr_t)16383u);
 struct gm_memory_io io={NULL,physical,flush,clean,barrier};
 memset(shared,0xa5,GN_HFI_BYTES);assert(gm_memory_bind_hfi(&m,shared,GN_HFI_BYTES,fw.hfi_address));
 assert(!gm_memory_bind_hfi(&m,shared,GN_HFI_BYTES,fw.hfi_address));assert(gm_memory_prepare(&m,&fw,&io));
 for(unsigned i=0;i<GN_HFI_BYTES;i++)assert(shared[i]==0);
 for(unsigned off=0;off<GN_HFI_BYTES;off+=4096){uint64_t pa=0;assert(gt_lookup(&m.table_plan,m.tables,fw.hfi_address+off,&pa)==1&&pa==0x920000000ULL+off);}
 assert(gm_memory_owned_va(&m,fw.hfi_address,GN_HFI_BYTES)==shared);
 assert(gm_memory_owned_va(&m,fw.hfi_address+GN_HFI_BYTES-1,1)==shared+GN_HFI_BYTES-1);
 assert(!gm_memory_owned_va(&m,fw.hfi_address+GN_HFI_BYTES-1,2));
 assert(gn_hfi_init(shared,GN_HFI_BYTES,fw.hfi_address)==GN_OK);
 struct gn_hfi_table*t=(struct gn_hfi_table*)(void*)shared;assert(t->queues==3&&t->queue[0].start==fw.hfi_address+GN_HFI_QUEUE0);
 assert(gm_memory_publish(&m,&io)&&flushes==1&&cleans==m.table_plan.table_count&&barriers==1);
 assert(!gm_memory_owned_va(&m,fw.hfi_address,1));
 memset(&m,0,sizeof(m));assert(!gm_memory_bind_hfi(&m,shared+1,GN_HFI_BYTES,fw.hfi_address));assert(!gm_memory_bind_hfi(&m,shared,4096,fw.hfi_address));
 assert(!gm_memory_bind_hfi(&m,&m,GN_HFI_BYTES,fw.hfi_address));
 assert(gm_memory_bind_hfi(&m,shared,GN_HFI_BYTES,fw.hfi_address+4096));assert(!gm_memory_prepare(&m,&fw,&io));
 puts("G19_EXTERNAL_HFI_MAPPING_PASS dynamicPages=4 staticFirmwarePreserved=1 noSharedCacheMaintenance=1");return 0;
}
