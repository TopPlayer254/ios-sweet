/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_g24_record.h"
#include "ziyi_g23_startup.h"
static void g24r_put(uint8_t*b,unsigned n,uint32_t x){for(unsigned j=0;j<4;j++)b[4*n+j]=(uint8_t)(x>>(8*j));}
static uint32_t g24r_get(const uint8_t*b,unsigned n){uint32_t x=0;for(unsigned j=0;j<4;j++)x|=(uint32_t)b[4*n+j]<<(8*j);return x;}
int g24_record_valid(const uint8_t*b,size_t n){
 if(!b||n!=G24_RECORD_BYTES||g24r_get(b,0)!=0x31504347||g24r_get(b,1)!=1||g24r_get(b,2)!=n||g24r_get(b,3)!=gs_crc(b+16,n-16))return 0;
 if(g24r_get(b,4)>G24_REFUSED||g24r_get(b,5)>G24_REFUSED||g24r_get(b,7)>127||g24r_get(b,79))return 0;
 if(g24r_get(b,4)==G24_DONE&&(g24r_get(b,6)||g24r_get(b,7)!=127||g24r_get(b,24)!=103||g24r_get(b,25)!=103||g24r_get(b,41)!=4096||g24r_get(b,43)!=1))return 0;
 return 1;
}
__attribute__((noinline)) int g24_record_pack(const struct g24_workspace*w,const struct g24_native_facts*f,uint8_t*out,size_t n){
 if(!w||!f||!out||n!=G24_RECORD_BYTES)return 0;
 uint8_t b[G24_RECORD_BYTES]={0};const struct g24_record*r=&w->r;
 g24r_put(b,0,0x31504347);g24r_put(b,1,1);g24r_put(b,2,G24_RECORD_BYTES);
 g24r_put(b,4,r->phase);
 g24r_put(b,5,r->failure_phase);
 g24r_put(b,6,r->error);
 g24r_put(b,7,r->proofs);
 g24r_put(b,8,r->reads);
 g24r_put(b,9,r->writes);
 g24r_put(b,10,r->pending_space);
 g24r_put(b,11,r->pending_index);
 g24r_put(b,12,r->pending_value);
 g24r_put(b,13,r->hardware_attempted);
 g24r_put(b,14,r->power_grant);
 g24r_put(b,15,r->sptprac);
 g24r_put(b,16,r->gdsc);
 g24r_put(b,17,r->profile_count);
 g24r_put(b,18,r->profile_cursor);
 g24r_put(b,19,r->smmu_cursor);
 g24r_put(b,20,r->cp_cursor);
 g24r_put(b,21,r->ring_words);
 g24r_put(b,22,r->blit_begin);
 g24r_put(b,23,r->blit_words);
 g24r_put(b,24,r->rptr);
 g24r_put(b,25,r->wptr);
 g24r_put(b,26,r->status);
 g24r_put(b,27,r->int_status);
 g24r_put(b,28,r->cp_fault);
 g24r_put(b,29,r->cp_protect);
 g24r_put(b,30,r->smmu_fsr);
 g24r_put(b,31,r->smmu_fsynr);
 g24r_put(b,32,r->smmu_far_lo);
 g24r_put(b,33,r->smmu_far_hi);
 g24r_put(b,34,r->global_fsr);
 g24r_put(b,35,r->core_saved);
 g24r_put(b,36,r->profile_saved);
 g24r_put(b,37,r->route_live);
 g24r_put(b,38,r->cp_started);
 g24r_put(b,39,r->pas_authenticated);
 g24r_put(b,40,r->fence_value);
 g24r_put(b,41,r->verified_pixels);
 g24r_put(b,42,r->first_bad_byte);
 g24r_put(b,43,r->presented);
 g24r_put(b,44,r->retained);
 g24r_put(b,45,r->program_crc);
 g24r_put(b,46,r->pixel_crc);
 g24r_put(b,47,r->guard_crc);
 g24r_put(b,48,r->proofs);
 g24r_put(b,49,r->verify_code);
 g24r_put(b,50,r->last_space);
 g24r_put(b,51,r->last_index);
 g24r_put(b,52,r->last_value);
 g24r_put(b,53,r->last_valid);
 g24r_put(b,54,(uint32_t)r->epoch);
 g24r_put(b,55,(uint32_t)(r->epoch>>32));
 g24r_put(b,56,(uint32_t)r->start);
 g24r_put(b,57,(uint32_t)(r->start>>32));
 g24r_put(b,58,(uint32_t)r->end);
 g24r_put(b,59,(uint32_t)(r->end>>32));
 g24r_put(b,60,w->pas.state);
 g24r_put(b,61,w->pas.error);
 g24r_put(b,62,w->pas.calls);
 g24r_put(b,63,w->pas.interrupts);
 g24r_put(b,64,w->pas.metadata_retained);
 g24r_put(b,65,w->pas.image_retained);
 g24r_put(b,66,(uint32_t)w->pas.last[0]);
 g24r_put(b,67,(uint32_t)(w->pas.last[0]>>32));
 g24r_put(b,68,(uint32_t)w->pas.last[1]);
 g24r_put(b,69,(uint32_t)(w->pas.last[1]>>32));
 g24r_put(b,70,f->zap_crc);
 g24r_put(b,71,f->zap_backed);
 g24r_put(b,72,f->zap_copied);
 g24r_put(b,73,f->guard_error);
 g24r_put(b,74,r->initial[G24_SMR]);
 g24r_put(b,75,r->initial[G24_S2CR]);
 g24r_put(b,76,r->initial[G24_SCTLR]);
 g24r_put(b,77,r->initial[G24_RPTR]);
 g24r_put(b,78,r->initial[G24_WPTR]);
 g24r_put(b,3,gs_crc(b+16,G24_RECORD_BYTES-16));if(!g24_record_valid(b,sizeof(b)))return 0;for(unsigned i=0;i<G24_RECORD_BYTES;i++)out[i]=b[i];return 1;
}
