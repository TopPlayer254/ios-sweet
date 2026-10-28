/* SPDX-License-Identifier: GPL-2.0-only
 * PAS ABI from Xiaomi 24856283 qcom_scm.c and qcom_scm-smc.c.
 * X0 interrupted continuation carries A6, while original X1..X5 arguments
 * remain intact. This transport owns no MMIO, allocation or firmware stores.
 */
#include "native.h"
#define SIP (2ull << 24)
#define SMC64 (1ull << 30)
#define INFO 0x0601u
#define INIT 0x0201u
#define MEM 0x0202u
#define START 0x0205u
#define STOP 0x0206u
#define SUPPORTED 0x0207u

static int fail(struct zwn_scm*s,unsigned error,unsigned poison)
{ if(!s->error)s->error=error;s->poison|=poison;return 0; }

static int call(struct zwn_scm*s,const struct zwn_io*io,unsigned operation,
 uint32_t function,uint32_t arginfo,uint64_t a,uint64_t b,uint64_t c)
{
 uint64_t session=0;
 s->operation=operation;
 for(unsigned attempt=0;attempt<100;attempt++){
  if(!io||!io->smc||!io->authority||!s->epoch||s->poison||
     !io->authority(io->ctx,s->epoch,operation))return fail(s,ZWN_OWNER,attempt!=0);
  uint64_t r[8]={attempt?1:SIP|SMC64|function,arginfo,a,b,c,0,session,0};
  io->smc(io->ctx,r);s->calls++;
  for(unsigned i=0;i<8;i++)s->last[i]=r[i];
  session=r[6];
  if(r[0]==1){s->interrupts++;continue;}
  if(r[0]==2||r[0]==3)return fail(s,ZWN_WAITQ,1);
  if((int32_t)r[0]==-12)return fail(s,ZWN_BUSY,operation!=ZWN_PAS_PROBE);
  if(r[0]!=0)return fail(s,ZWN_SCM,operation!=ZWN_PAS_PROBE);
  return 1;
 }
 return fail(s,ZWN_INTERRUPTS,1);
}

int zwn_pas_probe(struct zwn_scm*s,uint64_t epoch,const struct zwn_io*io)
{
 if(!s||s->epoch||!epoch||!io||!io->authority||!io->smc)return 0;
 *s=(struct zwn_scm){.epoch=epoch};
 /* The exact ARM64 convention probe, then availability of PAS_SUPPORTED. No
  * undocumented forced convention or legacy fallback on this 64-bit target. */
 if(!call(s,io,ZWN_PAS_PROBE,INFO,1,SIP|INFO,0,0))return 0;
 if(s->last[1]!=1)return fail(s,ZWN_SERVICE,0);
 s->convention64=1;
 if(!call(s,io,ZWN_PAS_PROBE,INFO,1,SIP|SUPPORTED,0,0))return 0;
 if(s->last[1]!=1)return fail(s,ZWN_SERVICE,0);
 if(!call(s,io,ZWN_PAS_PROBE,SUPPORTED,1,ZWN_PAS,0,0))return 0;
 if(!s->last[1])return fail(s,ZWN_SERVICE,0);
 s->available=1;return 1;
}

static int ready(const struct zwn_scm*s)
{ return s&&s->epoch&&s->convention64&&s->available&&!s->error&&!s->poison&&!s->stopped; }

int zwn_pas_init(struct zwn_scm*s,uint64_t pa,uint32_t allocation,uint32_t bytes,const struct zwn_io*io)
{
 if(!ready(s)||s->initialized||!pa||(pa&4095)||allocation<bytes||!bytes||
    allocation>0x10000u||(allocation&4095)||pa>UINT64_MAX-allocation)return 0;
 /* Retain the allocation before handing its address to secure world, even if
  * the call later fails or has an interrupted/WAITQ session. */
 s->metadata_retained=1;
 if(!call(s,io,ZWN_PAS_INIT,INIT,2u|(2u<<6),ZWN_PAS,pa,0))return 0;
 if(s->last[1])return fail(s,ZWN_SCM,1);
 s->initialized=1;return 1;
}

int zwn_pas_mem_setup(struct zwn_scm*s,uint32_t bytes,const struct zwn_io*io)
{
 if(!ready(s)||!s->initialized||s->memory_prepared||!bytes||bytes>ZWN_WPSS_BYTES||(bytes&4095))return 0;
 if(!call(s,io,ZWN_PAS_MEM,MEM,3,ZWN_PAS,ZWN_WPSS_IMAGE,bytes))return 0;
 if(s->last[1])return fail(s,ZWN_SCM,1);
 s->memory_prepared=1;s->image_bytes=bytes;return 1;
}

int zwn_pas_start(struct zwn_scm*s,const struct zwn_io*io)
{
 if(!ready(s)||!s->initialized||!s->memory_prepared||s->started)return 0;
 if(!call(s,io,ZWN_PAS_START,START,1,ZWN_PAS,0,0))return 0;
 if(s->last[1])return fail(s,ZWN_SCM,1);
 s->started=1;return 1;
}

int zwn_pas_shutdown(struct zwn_scm*s,int fresh_stop_ack,const struct zwn_io*io)
{
 if(!ready(s)||!s->started||fresh_stop_ack!=1)return 0;
 if(!call(s,io,ZWN_PAS_STOP,STOP,1,ZWN_PAS,0,0))return 0;
 if(s->last[1])return fail(s,ZWN_SCM,1);
 s->stopped=1;s->started=0;s->metadata_retained=0;return 1;
}
