/* SPDX-License-Identifier: GPL-2.0-only */
#include "native.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct model {uint64_t calls[110][8];unsigned n,deny,interrupt,waitq,busy,pas_error;uint64_t unavailable;};
static int authority(void*ctx,uint64_t epoch,unsigned operation)
{ struct model*m=ctx;return epoch==7&&operation!=m->deny; }
static void smc(void*ctx,uint64_t r[8])
{
 struct model*m=ctx;assert(m->n<110);memcpy(m->calls[m->n++],r,64);
 uint64_t f=r[0];assert((f&0xffff)!=0x207);
 if(m->n==1)assert(f==0xc2000601); /* Exact atomic convention probe. */
 else if(f!=1)assert(!(f&(1ull<<31))); /* No fast PAS commands. */
 if(m->interrupt&&m->n==1){r[0]=1;r[6]=0x1234;return;}
 if(f==1){assert(r[6]==0x1234);f=m->calls[0][0];}
 if(m->waitq){r[0]=m->waitq;r[1]=55;r[2]=66;return;}
 if(m->busy){r[0]=(uint64_t)(int64_t)-12;return;}
 r[0]=0;r[1]=(f&0xffff)==0x601?1:m->pas_error;
 if((f&0xffff)==0x601&&m->unavailable&&r[2]==m->unavailable)r[1]=0;
}
static struct zwn_io io(struct model*m){return (struct zwn_io){.ctx=m,.smc=smc,.authority=authority};}
static void pas(void)
{
 struct model m={0};struct zwn_io x=io(&m);struct zwn_scm s={0};
 assert(!zwn_pas_start(&s,&x)&&m.n==0);
 assert(zwn_pas_probe(&s,7,&x)&&m.n==5);
 assert(m.calls[0][0]==0xc2000601&&m.calls[0][2]==0x02000601);
 const unsigned expected[]={0x201,0x202,0x205,0x206};
 for(unsigned i=0;i<4;i++)assert(m.calls[i+1][0]==0x42000601&&m.calls[i+1][2]==(0x02000000u|expected[i]));
 assert(!zwn_pas_init(&s,0x900000001ull,16384,4364,&x)&&m.n==5);
 assert(!zwn_pas_init(&s,0x900000000ull,4096,4364,&x)&&m.n==5);
 assert(zwn_pas_init(&s,0x900000000ull,16384,4364,&x));
 assert(m.calls[5][0]==0x42000201&&m.calls[5][1]==0x82&&m.calls[5][3]==0x900000000ull);
 assert(!zwn_pas_start(&s,&x));
 assert(!zwn_pas_mem_setup(&s,ZWN_WPSS_BYTES+4096,&x));
 assert(zwn_pas_mem_setup(&s,ZWN_WPSS_BYTES,&x));
 assert(m.calls[6][0]==0x42000202&&m.calls[6][2]==6&&m.calls[6][3]==ZWN_WPSS_IMAGE&&m.calls[6][4]==ZWN_WPSS_BYTES);
 assert(zwn_pas_start(&s,&x)&&s.started&&s.metadata_retained);
 assert(!zwn_pas_start(&s,&x)&&!zwn_pas_shutdown(&s,0,&x)&&m.n==8);
 assert(zwn_pas_shutdown(&s,1,&x)&&s.stopped&&!s.metadata_retained&&m.n==9);
 assert(!zwn_pas_init(&s,0x900000000ull,16384,4364,&x));
 m=(struct model){.interrupt=1};x=io(&m);s=(struct zwn_scm){0};
 assert(zwn_pas_probe(&s,7,&x)&&m.n==6&&s.interrupts==1);
 assert(m.calls[1][0]==1&&m.calls[1][1]==m.calls[0][1]&&m.calls[1][2]==m.calls[0][2]&&m.calls[1][6]==0x1234);
 for(unsigned reason=2;reason<=3;reason++){
  m=(struct model){0};x=io(&m);s=(struct zwn_scm){0};assert(zwn_pas_probe(&s,7,&x));m.waitq=reason;
  assert(!zwn_pas_init(&s,0x900000000ull,16384,4364,&x)&&s.poison&&s.error==ZWN_WAITQ&&s.metadata_retained&&m.n==6);
  assert(!zwn_pas_mem_setup(&s,ZWN_WPSS_BYTES,&x)&&m.n==6);
 }
 m=(struct model){0};x=io(&m);s=(struct zwn_scm){0};assert(zwn_pas_probe(&s,7,&x));m.busy=1;
 assert(!zwn_pas_init(&s,0x900000000ull,16384,4364,&x)&&s.poison&&s.error==ZWN_BUSY&&m.n==6);
 m=(struct model){0};x=io(&m);s=(struct zwn_scm){0};assert(zwn_pas_probe(&s,7,&x));m.pas_error=1;
 assert(!zwn_pas_init(&s,0x900000000ull,16384,4364,&x)&&s.poison&&s.error==ZWN_SCM&&s.metadata_retained);
 for(unsigned i=0;i<4;i++){
  m=(struct model){.unavailable=0x02000000u|expected[i]};x=io(&m);s=(struct zwn_scm){0};
  assert(!zwn_pas_probe(&s,7,&x)&&s.error==ZWN_SERVICE&&!s.available&&!s.metadata_retained&&m.n==i+2);
 }
 m=(struct model){.deny=ZWN_PAS_PROBE};x=io(&m);s=(struct zwn_scm){0};
 assert(!zwn_pas_probe(&s,7,&x)&&m.n==0);
}
static void put(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static void smp(void)
{
 uint8_t p[344]={0},o[344];struct zwn_smp_view v;
 put(p,0x504d5324);put(p+4,0x123401);put(p+8,13u<<16);put(p+12,16u|(2u<<16));put(p+16,0xdeadbeef);
 memcpy(p+20,"master-kernel",14);put(p+36,0x12345678);memcpy(p+40,"wlan",5);put(p+56,0xaabbccdd);
 assert(zwn_smp_parse(p,sizeof(p),0,13,"master-kernel",&v)&&v.stop_offset==36&&v.features==0x1234);
 assert(zwn_smp_prepare_stop(p,sizeof(p),o,sizeof(o),1,&v));
 assert(v.state==0x12345679&&o[36]==(p[36]|1));
 for(unsigned i=0;i<sizeof(p);i++)if(i!=36)assert(p[i]==o[i]);
 assert(!zwn_smp_prepare_stop(o,sizeof(o),o,sizeof(o),1,&v));
 assert(zwn_smp_prepare_stop(o,sizeof(o),o,sizeof(o),0,&v)&&!memcmp(p,o,sizeof(p)));
 assert(!zwn_smp_prepare_stop(p,sizeof(p),p+1,sizeof(p),1,&v));
 assert(!zwn_smp_parse(p,339,0,13,"master-kernel",&v));
 assert(!zwn_smp_parse(p,sizeof(p),13,0,"master-kernel",&v));
 p[14]=17;assert(!zwn_smp_parse(p,sizeof(p),0,13,"master-kernel",&v));p[14]=2;
 memcpy(p+40,p+20,16);assert(!zwn_smp_parse(p,sizeof(p),0,13,"master-kernel",&v));
 memset(p+20,0,40);put(p+8,13);p[14]=1;memcpy(p+20,"slave-kernel",13);put(p+36,0x10e);
 assert(zwn_smp_parse(p,sizeof(p),13,0,"slave-kernel",&v)&&v.ready&&v.handover&&v.stop_ack&&!v.fatal);
}
int main(void){pas();smp();puts("WPSS_PAS_AND_SMP_TRANSPORT_PASS (model only; no native radio)");return 0;}
