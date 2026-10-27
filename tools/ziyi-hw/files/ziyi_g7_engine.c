/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_g7_engine.h"
#include "ziyi_g7_sha256.h"
#define GPUCC 0x03d90000ULL
#define GMU 0x03d6a000ULL
#define SMMU 0x03da0000ULL
#define ITCM 0x03d6d000ULL
#define DTCM 0x03d71000ULL
static const uint32_t cb_words[G7_CB_WORDS]={0,4,0x10,0x20,0x24,0x28,0x2c,0x30,0x38,0x3c,0x58,0x60,0x64,0x68,0x6c};
static int fail(struct g7_record*r,unsigned error,unsigned poison){if(!r->error)r->error=error;r->poison|=poison;return 0;}
static int inside(uint64_t at,uint64_t base,uint64_t bytes){return at>=base&&at-base<bytes;}
static int overlap(const void*a,size_t an,const void*b,size_t bn){uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;if(an>UINTPTR_MAX-x||bn>UINTPTR_MAX-y)return 1;return x<y+bn&&y<x+an;}
int g7_read_allowed(const struct g7_record*r,uint64_t address){
 if(!r||r->version!=1||r->bytes!=sizeof(*r)||r->poison||!(r->flags&G7_LOCKED)||(address&3))return 0;
 if(address==GPUCC+0x953c||address==GPUCC+0x905c||address==GMU+0x14000||address==GMU+0x14600||address==GMU+0x14070)return 1;
 if(!(r->flags&G7_QUIET))return 0;
 if(address>=SMMU&&address<SMMU+0x20000){
  uint32_t off=(uint32_t)(address-SMMU);
  if(off==0||off==0x20||off==0x24||off==0x28)return 1;
  if((off>=0x800&&off<0x818)||(off>=0xc00&&off<0xc18))return 1;
  if(!(r->flags&G7_PLAN)||r->plan.cb!=2||r->plan.smr!=0||r->plan.software_context_map!=7||r->plan.hardware_write_authority)return 0;
  if(off==0x1008||off==0x1808)return 1;
  for(unsigned i=0;i<G7_CB_WORDS;i++)if(off==0x12000+cb_words[i])return 1;
  return 0;
 }
 if(!(r->flags&G7_CONTEXT))return 0;
 if((r->flags&G7_CONTROLS_SAVED)&&(inside(address,ITCM,0x4000)||inside(address,DTCM,0x4000)))return 1;
 for(unsigned i=0;i<G7_CONTROLS;i++)if(address==GMU+g7_control_offsets[i])return 1;
 return 0;
}
static uint32_t read_word(struct g7_record*r,const struct g7_io*io,uint64_t address){
 if(r->error)return 0;
 r->last_address=address;
 if(!g7_read_allowed(r,address)){fail(r,G7_SCOPE,1);return 0;}
 int ok=0;uint32_t value=io->read(io->ctx,address,&ok);r->reads++;r->last_value=value;
 if(!ok)fail(r,G7_IO,1);return value;
}
static int quiet(struct g7_record*r,const struct g7_io*io){
 uint32_t cx=read_word(r,io,GPUCC+0x953c),gx=read_word(r,io,GPUCC+0x905c);
 uint32_t reset=read_word(r,io,GMU+0x14000),hfi=read_word(r,io,GMU+0x14600);
 if(r->error)return 0;
 if(!(cx&0x80000000u)||(gx&0x80000000u)||reset!=1||(hfi&1))return fail(r,G7_PREREQ,1);
 return 1;
}
static uint32_t crc32(const void*data,size_t bytes){const uint8_t*p=data;uint32_t c=~0u;for(size_t i=0;i<bytes;i++){c^=p[i];for(unsigned k=0;k<8;k++)c=(c>>1)^((0u-(c&1))&0xedb88320u);}return ~c;}
static int snapshot(struct g7_record*r,const struct g7_io*io,unsigned pass,int compare){
 const uint32_t gr0[]={0,0x20,0x24,0x28};
 for(unsigned i=0;i<4;i++){uint32_t v=read_word(r,io,SMMU+gr0[i]);if(r->error)return 0;if(compare&&r->snapshot.gr0[0][i]!=v)return fail(r,G7_CHANGED,1);r->snapshot.gr0[pass][i]=v;}
 for(unsigned i=0;i<6;i++){
  uint32_t a=read_word(r,io,SMMU+0x800+4*i),b=read_word(r,io,SMMU+0xc00+4*i);if(r->error)return 0;
  if(compare&&(r->snapshot.smr[0][i]!=a||r->snapshot.s2cr[0][i]!=b))return fail(r,G7_CHANGED,1);
  r->snapshot.smr[pass][i]=a;r->snapshot.s2cr[pass][i]=b;
  if((a&0x80000000u)||b!=0x20000)return fail(r,G7_ACTIVE,1);
 }
 return 1;
}
static int context(struct g7_record*r,const struct g7_io*io,unsigned pass,int compare){
 for(unsigned i=0;i<2;i++){
  uint32_t value=read_word(r,io,SMMU+(i?0x1808:0x1008));if(r->error)return 0;
  if(compare&&r->gr1[0][i]!=value)return fail(r,G7_CHANGED,1);r->gr1[pass][i]=value;
 }
 for(unsigned i=0;i<G7_CB_WORDS;i++){
  uint32_t value=read_word(r,io,SMMU+0x12000+cb_words[i]);if(r->error)return 0;
  if(compare&&r->context[0][i]!=value)return fail(r,G7_CHANGED,1);r->context[pass][i]=value;
 }
 if((r->context[pass][0]&1)||r->context[pass][10])return fail(r,G7_ACTIVE,1);
 return 1;
}
int g7_run_acquire(struct g7_record*r,const struct g7_prereq*q,const struct g7_buffers*b,const struct g7_io*io){
 if(!r||!q||!b||!io||!io->read||!io->ticks||!io->lease||!io->owned_buffer||!io->barrier)return 0;
 if(overlap(r,sizeof(*r),q,sizeof(*q))||overlap(r,sizeof(*r),b,sizeof(*b))||overlap(r,sizeof(*r),io,sizeof(*io)))return 0;
 *r=(struct g7_record){.version=1,.bytes=sizeof(*r),.epoch=q->epoch};
 if(q->board!=G7_BOARD||q->hlos_cb!=6||q->hlos_smr!=6||q->handoff_pairs||q->mapped_device_io!=1||!q->epoch||q->current_el!=4||(q->sctlr&0x3000000))return fail(r,G7_PREREQ,0);
 if(!((q->contract_kind==G7_CONTRACT_LIVE_FDT&&q->execution_kind==G7_EXEC_FLAT_OFF&&!(q->sctlr&0x1005))||(q->contract_kind==G7_CONTRACT_XNU_SECURE_DT&&q->execution_kind==G7_EXEC_KERNEL_ON&&(q->sctlr&0x1005)==0x1005)))return fail(r,G7_PREREQ,0);
 const uint32_t lo[]={18600000,1000000,180000000,135000000},hi[]={19800000,300000000,220000000,165000000};
 for(unsigned i=0;i<4;i++)if(q->rates[i]<lo[i]||q->rates[i]>hi[i])return fail(r,G7_PREREQ,0);
 r->start=io->ticks(io->ctx);if(r->start<q->epoch||r->start-q->epoch>19200000)return fail(r,G7_PREREQ,0);
 if(!b->copy0||!b->copy1||b->bytes0<G7_TCM_BYTES||b->bytes1<G7_TCM_BYTES||overlap(b->copy0,G7_TCM_BYTES,b->copy1,G7_TCM_BYTES)||overlap(b->copy0,G7_TCM_BYTES,r,sizeof(*r))||overlap(b->copy1,G7_TCM_BYTES,r,sizeof(*r))||overlap(b->copy0,G7_TCM_BYTES,q,sizeof(*q))||overlap(b->copy1,G7_TCM_BYTES,q,sizeof(*q))||overlap(b->copy0,G7_TCM_BYTES,b,sizeof(*b))||overlap(b->copy1,G7_TCM_BYTES,b,sizeof(*b))||overlap(b->copy0,G7_TCM_BYTES,io,sizeof(*io))||overlap(b->copy1,G7_TCM_BYTES,io,sizeof(*io))||!io->owned_buffer(io->ctx,b->copy0,G7_TCM_BYTES)||!io->owned_buffer(io->ctx,b->copy1,G7_TCM_BYTES))return fail(r,G7_BUFFER,0);
 if(!io->lease(io->ctx,q->epoch,1))return fail(r,G7_LEASE,0);r->flags=G7_LOCKED;r->phase=1;
 if(!quiet(r,io))goto done;r->flags|=G7_QUIET;
 r->snapshot=(struct ga_snapshot){.facts=255,.dt_cb=6,.dt_smr=6,.epoch=q->epoch};r->phase=2;
 if(!snapshot(r,io,0,0)||!snapshot(r,io,1,1))goto done;r->flags|=G7_SNAPSHOT;
 if(ga_plan_fresh(&r->snapshot,3,&r->plan)!=GA_OK||r->plan.cb!=2||r->plan.smr!=0||r->plan.hardware_write_authority){fail(r,G7_OWNER,1);goto done;}r->flags|=G7_PLAN;r->phase=3;
 if(!quiet(r,io)||!context(r,io,0,0)||!context(r,io,1,1))goto done;r->flags|=G7_CONTEXT;r->phase=4;
 for(unsigned pass=0;pass<2;pass++)for(unsigned i=0;i<G7_CONTROLS;i++){
  uint32_t value=read_word(r,io,GMU+g7_control_offsets[i]);if(r->error)goto done;
  if(pass&&r->controls[0][i]!=value){fail(r,G7_CHANGED,1);goto done;}r->controls[pass][i]=value;
 }
 r->flags|=G7_CONTROLS_SAVED;r->phase=5;
 for(unsigned pass=0;pass<2;pass++){
  uint8_t *copy=pass?b->copy1:b->copy0;if(!quiet(r,io))goto done;
  for(unsigned off=0;off<G7_TCM_BYTES;off+=4){
   if(!(off&0xfff)&&!quiet(r,io))goto done;
   uint64_t address=off<0x4000?ITCM+off:DTCM+off-0x4000;
   uint32_t value=read_word(r,io,address);if(r->error)goto done;
   for(unsigned k=0;k<4;k++)copy[off+k]=(uint8_t)(value>>(8*k));
  }
  io->barrier(io->ctx);r->tcm_crc[pass]=crc32(copy,G7_TCM_BYTES);g7_sha256(copy,G7_TCM_BYTES,r->tcm_sha256[pass]);
 }
 for(unsigned i=0;i<G7_TCM_BYTES;i++)if(b->copy0[i]!=b->copy1[i]){fail(r,G7_CHANGED,1);goto done;}
 r->tcm_equal=1;r->flags|=G7_TCM_SAVED;r->phase=6;
 if(!quiet(r,io)||!snapshot(r,io,1,1)||!context(r,io,1,1))goto done;r->flags|=G7_FINAL_QUIET;
done:
 r->end=io->ticks(io->ctx);
 if(r->end<r->start||r->end-r->start>19200000*5u)fail(r,G7_PREREQ,1);
 if(!io->lease(io->ctx,q->epoch,0))fail(r,G7_LEASE,1);else {r->lease_released=1;r->flags|=G7_RELEASED;}
 return !r->error&&!r->poison&&r->flags==511&&r->tcm_equal&&!r->device_writes;
}
int g7_firmware_ready(const struct g7_record*r){(void)r;return 0;}
