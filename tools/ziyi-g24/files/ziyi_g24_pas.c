/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_g24_pas.h"
#include "ziyi_g24_firmware.h"
static int g24p_fail(struct g24_pas*p,uint32_t e){p->state=G24P_REFUSED;if(!p->error)p->error=e;return 0;}
static int g24p_call(struct g24_pas*p,const struct g24_pas_io*o,unsigned op,uint32_t function,uint32_t info,uint64_t a,uint64_t b,uint64_t c){
 uint64_t session=0;int first_probe=op==G24P_INFO&&p->calls==0;
 for(unsigned n=0;n<100;n++){
  if(!o||!o->qualified||!o->smc||!o->qualified(o->ctx,p->epoch,op))return g24p_fail(p,1);
  uint64_t r[8]={n?1:0x42000000ULL|function|(first_probe?0x80000000ULL:0),info,a,b,c,0,session,0};
  p->calls++;if(!o->smc(o->ctx,r))return g24p_fail(p,2);
  for(unsigned i=0;i<8;i++)p->last[i]=r[i];session=r[6];
  if(r[0]==1){p->interrupts++;continue;}
  /* No WAITQ/EBUSY retry, no fake supported-peripheral result. */
  if(r[0])return g24p_fail(p,3);return 1;
 }
 return g24p_fail(p,4);
}
int g24_pas_probe(struct g24_pas*p,uint64_t epoch,const struct g24_pas_io*o){
 if(!p||p->state||p->epoch||!epoch)return 0;p->epoch=epoch;
 const uint32_t query[]={0x0601,0x0201,0x0202,0x0205};
 for(unsigned i=0;i<4;i++)if(!g24p_call(p,o,G24P_INFO,0x0601,1,0x02000000u|query[i],0,0)||p->last[1]!=1)return g24p_fail(p,5);
 p->state=G24P_PROBED;return 1;
}
int g24_pas_init(struct g24_pas*p,uint64_t pa,const struct g24_pas_io*o){
 if(!p||p->state!=G24P_PROBED||p->error||!pa||(pa&16383u)||pa>=(1ULL<<36))return 0;
 p->metadata_pa=pa;p->metadata_retained=1;
 if(!g24p_call(p,o,G24P_METADATA,0x0201,2u|(2u<<6),13,pa,0)||p->last[1])return g24p_fail(p,6);
 p->state=G24P_INIT;return 1;
}
int g24_pas_memory(struct g24_pas*p,const struct g24_pas_io*o){
 if(!p||p->state!=G24P_INIT||p->error)return 0;p->image_retained=1;
 if(!g24p_call(p,o,G24P_RANGE,0x0202,3,13,G24_ZAP_PA,G24_ZAP_LOAD_BYTES)||p->last[1])return g24p_fail(p,7);
 p->state=G24P_MEMORY;return 1;
}
int g24_pas_auth(struct g24_pas*p,const struct g24_pas_io*o){
 if(!p||p->state!=G24P_MEMORY||p->error)return 0;
 if(!g24p_call(p,o,G24P_START,0x0205,1,13,0,0)||p->last[1])return g24p_fail(p,8);
 p->state=G24P_AUTH;return 1;
}
