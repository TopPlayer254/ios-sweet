#include "smmu.h"
#include "u3_policy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct u3_smmu base,a,b;
static unsigned checks;
#define CHECK(x) do{assert(x);checks++;}while(0)
int main(void)
{
 base.cr0=0x280406;base.id0=0x4c017e7d;base.id1=0x6000004e;
 base.id2=0x5111;base.id7=0x24;base.smr[0]=0x80000020;
 base.cbar=0x1f000;base.cba2r=1;base.sctlr=0x0a9f00e0;base.tcr2=0x60;
 base.s2cr[1]=0x10000;a=b=base;
 CHECK(zws_storage_coexists(&a,&b,&base));
 b.smr[1]=0x80011c00;b.s2cr[1]=1;CHECK(!zws_storage_coexists(&a,&b,&base));
 CHECK(!zws_storage_lease(0,1,0,0));CHECK(!zws_storage_lease(1,0,0,0));
 CHECK(!zws_storage_lease(120,1,0,0));CHECK(!zws_storage_lease(1,78,0,0));
 CHECK(!zws_storage_lease(1,1,0x80000000,0));
 CHECK(zws_storage_lease(1,1,0,0x10000));CHECK(zws_storage_lease(1,1,0,0x10000));
 CHECK(!zws_storage_lease(2,1,0,0x10000));
 CHECK(zws_storage_coexists(&a,&b,&base));a=b;
 CHECK(zws_storage_coexists(&a,&b,&base));
 /* Every other byte of both snapshots remains protected. */
 for(unsigned i=0;i<sizeof(a);i++){
  struct u3_smmu changed=a;((unsigned char*)&changed)[i]^=0x80;
  CHECK(!zws_storage_coexists(&changed,&b,&base));
  CHECK(!zws_storage_coexists(&a,&changed,&base));
 }
 b=a;b.s2cr[1]=0;CHECK(!zws_storage_coexists(&a,&b,&base));
 b=a;b.smr[1]=0xffff1c00;CHECK(!zws_storage_coexists(&a,&b,&base));
 b=base;b.s2cr[1]=1;CHECK(zws_storage_coexists(&a,&b,&base));
 b=base;CHECK(zws_storage_coexists(&a,&b,&base));
 b=a;b.smr[2]=0x80011c00;CHECK(!zws_storage_coexists(&a,&b,&base));
 CHECK(!zws_storage_coexists(NULL,&b,&base));
 uint64_t initial=zws_storage_epoch(),token=zws_storage_write_begin();
 CHECK(!(initial&1)&&token==initial+1&&zws_storage_epoch()==token);
 CHECK(!zws_storage_write_begin());CHECK(!zws_storage_write_end(token+2));
 CHECK(zws_storage_epoch()==token);CHECK(zws_storage_write_end(token));
 CHECK(zws_storage_epoch()==initial+2);CHECK(!zws_storage_write_end(token));
 uint32_t wanted[ZWS_CB_WORDS]={0x127,0x38071,0x5200000,0x10009,0,0,0x803519,0xf404ff44,0,0x1f300,1},actual[ZWS_CB_WORDS];
 memcpy(actual,wanted,sizeof(actual));CHECK(zws_context_matches(actual,wanted));
 for(unsigned bits=0;bits<4;bits++){actual[0]=(wanted[0]&~6u)|(bits<<1);CHECK(zws_context_matches(actual,wanted));}
 for(unsigned word=0;word<ZWS_CB_WORDS;word++)for(unsigned bit=0;bit<32;bit++){
  memcpy(actual,wanted,sizeof(actual));actual[word]^=1u<<bit;
  CHECK(zws_context_matches(actual,wanted)==((word==0&&(bit==1||bit==2))||(word==6&&bit>=30)));
 }
 memcpy(actual,wanted,sizeof(actual));actual[9]&=~0x300u;CHECK(zws_context_matches(actual,wanted));
 /* Actual W7 readback now matches an explicitly requested W8 profile. */
 memcpy(actual,wanted,sizeof(actual));actual[2]=wanted[2]=0x48c8000;actual[3]=wanted[3]=0x50009;
 CHECK(zws_context_matches(actual,wanted));
 actual[0]=0x1127;CHECK(!zws_context_matches(actual,wanted));
 memcpy(actual,wanted,sizeof(actual));actual[1]=0x38011;CHECK(!zws_context_matches(actual,wanted));
 memcpy(actual,wanted,sizeof(actual));wanted[0]=0x1127;CHECK(!zws_context_matches(actual,wanted));wanted[0]=0x127;
 wanted[1]=0x38011;CHECK(!zws_context_matches(actual,wanted));wanted[1]=0x38071;
 CHECK(!zws_context_matches(NULL,wanted));CHECK(!zws_context_matches(wanted,NULL));
 printf("WIFI_UFS_EXACT_LEASE_PASS %u checks; no native claim\n",checks);
}
