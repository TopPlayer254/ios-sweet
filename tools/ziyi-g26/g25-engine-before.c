/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_g24_engine.h"
#include "ziyi_g23_startup.h"
#include <string.h>
static const uint64_t g24_ctl[G24_CTL_COUNT]={
 0x3d7e648,0x3d7e644,0x3d7e650,0x3d7e340,0x3d9905c,0x3d00840,0x3d00804,0x3d02084,0x3d02090,0x3d02018,0x3d0201c,0x3d02020,0x3d02000,0x3d02004,0x3d020c0,0x3d020c4,0x3d02008,0x3d02010,0x3d02014,
 0x3da0804,0x3da0c04,0x3da1000,0x3da1800,0x3db0000,0x3db0004,0x3db0010,0x3db0030,0x3db0020,0x3db0024,0x3db0028,0x3db002c,0x3db0038,0x3db003c,0x3db0058,0x3db0068,0x3db0060,0x3db0064,0x3db0610,0x3db07f0,0x3db07f4,0x3da0048,0x3da0800,0x3da0c00,0x3db2000,0x3d7e070,0x3d7e600,0x3d7e000,0x3d0f114
};
#include "ziyi_g24_profile.inc"
static const unsigned g24_smmu_order[]={G24_SCTLR,G24_CBA2R,G24_CBAR,G24_TCR2,G24_TCR,G24_TTBR1LO,G24_TTBR1HI,G24_TTBRLO,G24_TTBRHI,G24_MAIR0,G24_MAIR1,G24_SCTLR,G24_TLBI,G24_TLBSYNC,G24_S2CR,G24_SMR};
static const unsigned g24_cp_order[]={G24_RPTRLO,G24_RPTRHI,G24_RBCNTL,G24_RBLO,G24_RBHI,G24_SQELO,G24_SQEHI,G24_SQE_CONTROL};
_Static_assert(G24_PROFILE_COUNT<=G24_MAX_PROFILE,"profile capacity");
uint64_t g24_register_address(unsigned space,unsigned id){return space==G24_CTL?(id<G24_CTL_COUNT?g24_ctl[id]:0):space==G24_PROFILE&&id<G24_PROFILE_COUNT?g24_profile[id].address:0;}
unsigned g24_profile_count(void){return G24_PROFILE_COUNT;}
static int g24e_fail(struct g24_workspace*w,unsigned e){if(!w->r.error){w->r.error=e;w->r.failure_phase=w->r.phase;}w->r.phase=G24_REFUSED;w->r.retained=1;return 0;}
static int g24e_owner(struct g24_workspace*w,const struct g24_io*o){return o->qualified(o->ctx,w->r.epoch,w->r.phase);}
static int g24e_read(struct g24_workspace*w,const struct g24_io*o,unsigned sp,unsigned id,uint32_t*v){
 w->r.last_space=sp;w->r.last_index=id;w->r.last_valid=0;
 if(!g24_register_address(sp,id)||!g24e_owner(w,o)||!o->read(o->ctx,sp,id,v))return g24e_fail(w,G24_E_IO);w->r.last_value=*v;w->r.last_valid=1;w->r.reads++;return 1;
}
static int g24e_tick(struct g24_workspace*w,const struct g24_io*o,uint64_t*v){if(!o->now(o->ctx,v)||*v<w->r.end||*v-w->r.start>19200000ULL*15)return g24e_fail(w,G24_E_TIMEOUT);w->r.end=*v;return 1;}
static int g24e_target(const struct g24_workspace*w,unsigned id,uint32_t*out){
 switch(id){case G24_REQUEST:*out=0x40000000;break;case G24_ACK:*out=0x80000000;break;
 case G24_SMR:*out=0x84000000;break;case G24_S2CR:*out=0x300;break;case G24_CBAR:*out=0x1f300;break;case G24_CBA2R:*out=1;break;
 case G24_SCTLR:*out=w->r.smmu_cursor?0xc00127:0;break;case G24_TCR2:*out=0x38071;break;case G24_TCR:*out=0x803519;break;
 case G24_TTBRLO:*out=(uint32_t)w->memory.translations.table_physical[0];break;case G24_TTBRHI:*out=(uint32_t)(w->memory.translations.table_physical[0]>>32);break;
 case G24_TTBR1LO:case G24_TTBR1HI:case G24_TLBI:case G24_TLBSYNC:*out=0;break;case G24_MAIR0:*out=0xf404ff44;break;case G24_MAIR1:*out=0xefe4;break;
 case G24_SQELO:*out=w->memory.sqe_iova;break;case G24_RBLO:*out=w->memory.ring_iova;break;case G24_RBHI:case G24_SQEHI:case G24_RPTRHI:*out=0;break;
 case G24_RPTRLO:*out=w->memory.fence_iova+64;break;case G24_RBCNTL:*out=11u|(2u<<8);break;case G24_SQE_CONTROL:*out=1;break;case G24_WPTR:*out=w->r.ring_words;break;default:return 0;}
 return 1;
}
int g24_store_allowed(const struct g24_workspace*w,unsigned sp,unsigned id,uint32_t value,uint32_t current){
 if(!w||!w->r.epoch||w->r.error||!w->memory.valid||w->memory.epoch!=w->r.epoch)return 0;
 if(sp==G24_PROFILE){if(w->r.phase!=G24_CONFIG||!w->r.profile_saved||id!=w->r.profile_cursor||id>=G24_PROFILE_COUNT||current!=w->r.profile_current[id])return 0;const struct g24_reg*d=&g24_profile[id];return value==((current&~d->mask)|d->value);}
 if(sp!=G24_CTL)return 0;uint32_t want=0;if(!g24e_target(w,id,&want)||value!=want)return 0;
 if(id==G24_REQUEST)return w->r.phase==G24_POWER&&!w->r.hardware_attempted;
 if(id==G24_ACK)return w->r.phase==G24_POWER&&w->r.power_grant==0x80000000;
 if((id>=G24_SMR&&id<=G24_MAIR1)||id==G24_TLBI||id==G24_TLBSYNC)return w->r.phase==G24_SMMU&&w->r.core_saved&&!w->r.route_live&&w->r.smmu_cursor<16&&id==g24_smmu_order[w->r.smmu_cursor];
 if(id==G24_WPTR){unsigned end=w->r.phase==G24_ME_INIT?9:w->r.phase==G24_UNSECURE?11:w->r.phase==G24_FENCE?16:w->r.phase==G24_BLIT?103:0;return end&&value==end&&w->r.cp_started&&value>current;}
 return w->r.phase==G24_ME_INIT&&w->r.route_live&&w->r.cp_cursor<8&&id==g24_cp_order[w->r.cp_cursor];
}
static int g24e_write(struct g24_workspace*w,const struct g24_io*o,unsigned sp,unsigned id,uint32_t value,int readback){
 uint32_t current=0;if(id!=G24_REQUEST&&id!=G24_ACK&&!g24e_read(w,o,sp,id,&current))return 0;
 if(!g24_store_allowed(w,sp,id,value,current)||!g24e_owner(w,o))return g24e_fail(w,G24_E_OWNER);
 w->r.pending_space=sp;w->r.pending_index=id;w->r.pending_value=value;
 if(!o->write(o->ctx,sp,id,value))return g24e_fail(w,G24_E_IO);w->r.writes++;w->r.hardware_attempted=1;
 if(!o->order(o->ctx))return g24e_fail(w,G24_E_IO);
 if(readback){uint32_t got;if(!g24e_read(w,o,sp,id,&got)||got!=value)return g24e_fail(w,G24_E_CONFIG);}
 if(sp==G24_PROFILE)for(unsigned i=0;i<G24_PROFILE_COUNT;i++)if(g24_profile[i].address==g24_profile[id].address)w->r.profile_current[i]=value;
 return 1;
}
static int g24e_put(struct g24_workspace*w,const struct g24_io*o,unsigned id){uint32_t v;if(!g24e_target(w,id,&v))return g24e_fail(w,G24_E_ARGUMENT);return g24e_write(w,o,G24_CTL,id,v,id!=G24_TLBI&&id!=G24_TLBSYNC&&id!=G24_REQUEST&&id!=G24_ACK);}
static int g24e_faults(struct g24_workspace*w,const struct g24_io*o){
 if(!g24e_read(w,o,G24_CTL,G24_STATUS,&w->r.status)||!g24e_read(w,o,G24_CTL,G24_INT,&w->r.int_status)||!g24e_read(w,o,G24_CTL,G24_CPFAULT,&w->r.cp_fault)||!g24e_read(w,o,G24_CTL,G24_CPPROTECT,&w->r.cp_protect)||!g24e_read(w,o,G24_CTL,G24_FSR,&w->r.smmu_fsr)||!g24e_read(w,o,G24_CTL,G24_GLOBAL_FSR,&w->r.global_fsr))return 0;
 if(w->r.cp_fault||w->r.cp_protect||(w->r.int_status&0x13000282)||(w->r.smmu_fsr&0xc00001fe)||w->r.global_fsr){(void)g24e_read(w,o,G24_CTL,G24_FSYNR,&w->r.smmu_fsynr);(void)g24e_read(w,o,G24_CTL,G24_FARLO,&w->r.smmu_far_lo);(void)g24e_read(w,o,G24_CTL,G24_FARHI,&w->r.smmu_far_hi);return g24e_fail(w,G24_E_CP);}return 1;
}
static int g24e_submit(struct g24_workspace*w,const struct g24_io*o,const uint32_t*data,unsigned words){
 if(!words||words>256||w->r.ring_words>4096-words||!g24e_owner(w,o))return g24e_fail(w,G24_E_ARGUMENT);
 unsigned at=w->r.ring_words;for(unsigned i=0;i<words;i++){w->expected_ring[i]=data[i];w->ring[at+i]=data[i];}
 if(!o->order(o->ctx))return g24e_fail(w,G24_E_IO);
 for(unsigned i=0;i<words;i++)if(w->ring[at+i]!=w->expected_ring[i])return g24e_fail(w,G24_E_GUARD);
 w->r.ring_words+=words;w->r.program_crc=gs_crc((const uint8_t*)w->expected_ring,words*4);
 if(!g24e_put(w,o,G24_WPTR))return 0;uint64_t start,now;if(!g24e_tick(w,o,&start))return 0;
 for(unsigned n=0;n<20000;n++){
  if(!g24e_read(w,o,G24_CTL,G24_RPTR,&w->r.rptr)||!g24e_read(w,o,G24_CTL,G24_WPTR,&w->r.wptr)||!g24e_faults(w,o))return 0;
  if(w->r.rptr==w->r.ring_words&&w->r.wptr==w->r.ring_words&&!(w->r.status&(1u<<23)))return 1;
  if(!g24e_tick(w,o,&now)||now-start>=38400000||!o->delay(o->ctx,100))return g24e_fail(w,G24_E_TIMEOUT);
 }return g24e_fail(w,G24_E_TIMEOUT);
}
static uint32_t g24e_parity(uint32_t x){unsigned p=1;while(x){p^=x&1;x>>=1;}return p;}
static uint32_t g24e_pkt(uint32_t op,uint32_t n){return 0x70000000|n|(g24e_parity(n)<<15)|(op<<16)|(g24e_parity(op)<<23);}
__attribute__((noinline)) int g24_run(struct g24_workspace*w,const struct g24_io*o){
 if(!w||!o||!o->qualified||!o->read||!o->write||!o->order||!o->delay||!o->now||!o->zap_copy||!o->present||w->r.phase!=G24_READY||!w->ring||!w->pixels||!w->fence||!w->memory.valid||w->memory.epoch!=w->r.epoch)return 0;
 if(!g24e_owner(w,o)||!o->now(o->ctx,&w->r.start))return g24e_fail(w,G24_E_OWNER);w->r.proofs=GP_NATIVE_HFI|GP_NATIVE_VISIBILITY;w->r.end=w->r.start;w->r.profile_count=G24_PROFILE_COUNT;
 w->r.phase=G24_POWER;uint32_t v;if(!g24e_read(w,o,G24_CTL,G24_INFO,&v)||(v&0x80000000))return g24e_fail(w,G24_E_ACTIVE);
 if(!g24e_put(w,o,G24_REQUEST))return 0;
 for(unsigned n=0;n<5000;n++){if(!g24e_read(w,o,G24_CTL,G24_INFO,&v))return 0;if(v&0x80000000){w->r.power_grant=0x80000000;break;}if(!o->delay(o->ctx,100))return g24e_fail(w,G24_E_IO);}
 if(!w->r.power_grant)return g24e_fail(w,G24_E_TIMEOUT);
 if(!g24e_put(w,o,G24_ACK)||!g24e_read(w,o,G24_CTL,G24_SPTPRAC,&w->r.sptprac)||!g24e_read(w,o,G24_CTL,G24_GDSC,&w->r.gdsc)||(w->r.sptprac&0xc0)||!(w->r.gdsc&0x80000000))return g24e_fail(w,G24_E_ACTIVE);w->r.proofs|=GP_NATIVE_GX_POWER;
 w->r.phase=G24_SNAPSHOT;
 static const unsigned saved[]={G24_RPTR,G24_WPTR,G24_SQE_CONTROL,G24_RBLO,G24_RBHI,G24_SQELO,G24_SQEHI,G24_RBCNTL,G24_RPTRLO,G24_RPTRHI,G24_SMR,G24_S2CR,G24_CBAR,G24_CBA2R,G24_SCTLR,G24_ACTLR,G24_TCR2,G24_TCR,G24_TTBRLO,G24_TTBRHI,G24_TTBR1LO,G24_TTBR1HI,G24_MAIR0,G24_MAIR1};
 for(unsigned pass=0;pass<2;pass++)for(unsigned i=0;i<sizeof(saved)/sizeof(saved[0]);i++){unsigned id=saved[i];if(!g24e_read(w,o,G24_CTL,id,&v)||(pass&&v!=w->r.initial[id]))return g24e_fail(w,G24_E_ACTIVE);w->r.initial[id]=v;}
 if(w->r.initial[G24_RPTR]||w->r.initial[G24_WPTR]||w->r.initial[G24_RBLO]||w->r.initial[G24_RBHI]||w->r.initial[G24_SQELO]||w->r.initial[G24_SQEHI]||w->r.initial[G24_SMR]||w->r.initial[G24_S2CR]!=0x20000||w->r.initial[G24_CBAR]||w->r.initial[G24_CBA2R]||(w->r.initial[G24_SCTLR]&1))return g24e_fail(w,G24_E_ACTIVE);
 if(!g24e_faults(w,o)||(w->r.status&(1u<<23)))return g24e_fail(w,G24_E_ACTIVE);w->r.core_saved=1;
 w->r.phase=G24_SMMU;
 for(unsigned i=0;i<14;i++){w->r.smmu_cursor=i;if(!g24e_put(w,o,g24_smmu_order[i]))return 0;}
 for(unsigned n=0;n<10000;n++){if(!g24e_read(w,o,G24_CTL,G24_TLBSTATUS,&v))return 0;if(!v)break;if(v!=1||n==9999||!o->delay(o->ctx,1))return g24e_fail(w,G24_E_SMMU);}
 w->r.smmu_cursor=14;if(!g24e_put(w,o,G24_S2CR))return 0;w->r.smmu_cursor=15;if(!g24e_put(w,o,G24_SMR))return 0;w->r.route_live=1;w->r.proofs|=GP_NATIVE_SMMU;
 for(unsigned pass=0;pass<2;pass++)for(unsigned i=0;i<G24_PROFILE_COUNT;i++){if(!g24e_read(w,o,G24_PROFILE,i,&v)||(pass&&v!=w->r.profile_old[i]))return g24e_fail(w,G24_E_CONFIG);w->r.profile_old[i]=w->r.profile_current[i]=v;}w->r.profile_saved=1;
 w->r.phase=G24_CONFIG;
 for(unsigned i=0;i<G24_PROFILE_COUNT;i++){w->r.profile_cursor=i;const struct g24_reg*d=&g24_profile[i];v=(w->r.profile_current[i]&~d->mask)|d->value;if(!g24e_write(w,o,G24_PROFILE,i,v,1))return 0;}
 w->r.phase=G24_ME_INIT;
 for(unsigned i=0;i<8;i++){w->r.cp_cursor=i;if(!g24e_put(w,o,g24_cp_order[i]))return 0;}w->r.cp_started=1;
 uint32_t init[9]={0,0x2f,3,0x20000000,0,0,0,0,0};init[0]=g24e_pkt(0x48,8);if(!g24e_submit(w,o,init,9))return 0;w->r.proofs|=GP_NATIVE_SQE;
 w->r.phase=G24_ZAP;
 if(!g24_pas_probe(&w->pas,w->r.epoch,&o->pas)||!g24_pas_init(&w->pas,w->pas.metadata_pa,&o->pas)||!g24_pas_memory(&w->pas,&o->pas)||!o->zap_copy(o->ctx)||!g24_pas_auth(&w->pas,&o->pas))return g24e_fail(w,G24_E_PAS);w->r.pas_authenticated=1;w->r.proofs|=GP_NATIVE_ZAP;
 w->r.phase=G24_UNSECURE;uint32_t secure[2]={g24e_pkt(0x66,1),0};if(!g24e_submit(w,o,secure,2))return 0;
 w->r.phase=G24_FENCE;uint32_t token=0xc0240000u|(uint32_t)(w->r.epoch&0xffffu);w->fence[2]=0;if(!o->order(o->ctx))return g24e_fail(w,G24_E_IO);
 uint32_t fence[5]={g24e_pkt(0x3d,3),w->memory.fence_iova+8,0,token,g24e_pkt(0x26,0)};if(!g24e_submit(w,o,fence,5)||!o->order(o->ctx))return g24e_fail(w,G24_E_IO);w->r.fence_value=w->fence[2];if(w->r.fence_value!=token)return g24e_fail(w,G24_E_CP);w->r.proofs|=GP_NATIVE_CP_FENCE;
 w->r.phase=G24_BLIT;w->r.blit_begin=w->r.ring_words;
 uint32_t pixels[GP_WORDS];size_t words=0;if(gp_build(pixels,GP_WORDS,&w->pixel_request,&words)!=GP_OK||words!=GP_WORDS)return g24e_fail(w,G24_E_PIXELS);
 w->r.blit_words=(uint32_t)words;if(!g24e_submit(w,o,pixels,(unsigned)words)||!o->order(o->ctx))return g24e_fail(w,G24_E_IO);
 w->r.phase=G24_VERIFY;if(!g24e_owner(w,o))return g24e_fail(w,G24_E_OWNER);struct gp_observation observation={.epoch=w->pixel_request.epoch,.proof_flags=w->r.proofs,.rptr=w->r.rptr-w->r.blit_begin,.wptr=w->r.wptr-w->r.blit_begin,.color_token=w->fence[0],.flush_token=w->fence[1],.cp_fault=w->r.cp_fault,.smmu_fault=w->r.smmu_fsr&0xc00001fe,.gpu_busy=w->r.status&(1u<<23)};
 w->r.verify_code=(uint32_t)gp_verify(&w->pixel_request,&observation,w->pixels,GP_PAGE,w->guard,(volatile uint8_t*)w->fence+128,64,&w->r.first_bad_byte);if(w->r.verify_code!=GP_OK)return g24e_fail(w,w->r.verify_code==GP_GUARD?G24_E_GUARD:G24_E_PIXELS);
 w->r.verified_pixels=4096;w->r.pixel_crc=gs_crc((const uint8_t*)w->pixels,GP_PAGE);w->r.guard_crc=gs_crc(w->guard,64);
 w->r.phase=G24_PRESENT;if(!o->present(o->ctx))return g24e_fail(w,G24_E_DISPLAY);w->r.presented=1;uint64_t end;if(!g24e_tick(w,o,&end))return 0;w->r.phase=G24_DONE;w->r.retained=1;return 1;
}
