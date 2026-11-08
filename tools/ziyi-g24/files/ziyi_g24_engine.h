/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include "ziyi_g24_firmware.h"
#include "ziyi_g24_memory.h"
#include "ziyi_g24_pixel.h"
#include "ziyi_g24_pas.h"
enum {G24_NEW,G24_READY,G24_POWER,G24_SNAPSHOT,G24_SMMU,G24_CONFIG,G24_ME_INIT,G24_ZAP,G24_UNSECURE,G24_FENCE,G24_BLIT,G24_VERIFY,G24_PRESENT,G24_DONE,G24_REFUSED};
enum {G24_E_ARGUMENT=400,G24_E_OWNER,G24_E_IO,G24_E_TIMEOUT,G24_E_ACTIVE,G24_E_SMMU,G24_E_CONFIG,G24_E_CP,G24_E_PAS,G24_E_PIXELS,G24_E_GUARD,G24_E_DISPLAY};
enum {G24_CTL=0,G24_PROFILE=1};
enum {
 G24_INFO,G24_ACK,G24_REQUEST,G24_SPTPRAC,G24_GDSC,G24_STATUS,G24_INT,G24_CPFAULT,G24_CPPROTECT,G24_RPTR,G24_WPTR,G24_SQE_CONTROL,G24_RBLO,G24_RBHI,G24_SQELO,G24_SQEHI,G24_RBCNTL,G24_RPTRLO,G24_RPTRHI,
 G24_SMR,G24_S2CR,G24_CBAR,G24_CBA2R,G24_SCTLR,G24_ACTLR,G24_TCR2,G24_TCR,G24_TTBRLO,G24_TTBRHI,G24_TTBR1LO,G24_TTBR1HI,G24_MAIR0,G24_MAIR1,G24_FSR,G24_FSYNR,G24_FARLO,G24_FARHI,G24_TLBI,G24_TLBSYNC,G24_TLBSTATUS,G24_GLOBAL_FSR,G24_GMU_SMR,G24_GMU_S2CR,G24_GMU_SCTLR,G24_FW,G24_HFI,G24_RESET,G24_HALT,G24_CTL_COUNT
};
#define G24_MAX_PROFILE 160u
struct g24_reg {uint64_t address;uint32_t value,mask;};
struct g24_record {
 uint32_t phase,error,reads,writes,pending_space,pending_index,pending_value,hardware_attempted;
 uint32_t power_grant,sptprac,gdsc,profile_count,profile_cursor,smmu_cursor,ring_words,blit_begin,blit_words;
 uint32_t rptr,wptr,status,cp_fault,cp_protect,smmu_fsr,smmu_fsynr,smmu_far_lo,smmu_far_hi,global_fsr;
 uint32_t initial[G24_CTL_COUNT],profile_old[G24_MAX_PROFILE],profile_current[G24_MAX_PROFILE];
 uint32_t core_saved,profile_saved,route_live,cp_started,pas_authenticated,fence_value,verified_pixels,first_bad_byte,presented,retained;
 uint32_t program_crc,pixel_crc,guard_crc,proofs,cp_cursor,verify_code,int_status;
 uint32_t failure_phase,last_space,last_index,last_value,last_valid;
 uint64_t epoch,start,end;
};
/* GCK1 is auxiliary telemetry; the320-byte GCP1 ABI is unchanged. */
#define G28_CLOCK_BYTES 80u
enum {G28_CLOCK_NONE,G28_CLOCK_PROGRAM,G28_CLOCK_SKIP,G28_CLOCK_REFUSED};
struct g28_clock_diag {
 uint32_t mode,valid,clock_before,sp_before,sp_off,sp_now,control_now,clock_now,writes,stable_reads;
};
struct g24_workspace {
 struct g24_record r;struct g24_firmware fw;struct gc_plan memory;struct g24_pas pas;
 volatile uint32_t *ring,*fence;volatile uint8_t*pixels;
 struct gp_request pixel_request;
 uint8_t guard[64];uint32_t expected_ring[256];
 struct g28_clock_diag clock;
};
struct g24_io {
 void*ctx;int(*qualified)(void*,uint64_t,unsigned);
 int(*read)(void*,unsigned,unsigned,uint32_t*);
 int(*write)(void*,unsigned,unsigned,uint32_t);
 int(*order)(void*);int(*delay)(void*,uint32_t);int(*now)(void*,uint64_t*);
 int(*zap_copy)(void*);int(*present)(void*);struct g24_pas_io pas;
};
uint64_t g24_register_address(unsigned,unsigned);
unsigned g24_profile_count(void);
int g24_store_allowed(const struct g24_workspace*,unsigned,unsigned,uint32_t,uint32_t);
int g24_run(struct g24_workspace*,const struct g24_io*);

static inline uint32_t g28_clock_crc(const uint8_t*b,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;i++){c^=b[i];for(unsigned j=0;j<8;j++)c=(c>>1)^(0xedb88320u&(0u-(c&1u)));}return ~c;}
static inline int g28_clock_pack(const struct g24_workspace*w,uint8_t*b,size_t n){
 if(!w||!b||n!=G28_CLOCK_BYTES||w->clock.mode>3||w->clock.valid>63||w->clock.writes>52||w->clock.stable_reads>174)return 0;
 const struct g28_clock_diag*d=&w->clock;
 uint32_t a[20]={0x314b4347u,1,G28_CLOCK_BYTES,0,d->mode,d->valid,d->clock_before,d->sp_before,d->sp_off,d->sp_now,d->control_now,d->clock_now,d->writes,d->stable_reads,w->r.error,w->r.profile_cursor,(uint32_t)w->r.epoch,(uint32_t)(w->r.epoch>>32),0,0};
 for(unsigned i=0;i<20;i++)for(unsigned j=0;j<4;j++)b[4*i+j]=(uint8_t)(a[i]>>(8*j));
 uint32_t crc=g28_clock_crc(b+16,64);for(unsigned j=0;j<4;j++)b[12+j]=(uint8_t)(crc>>(8*j));return 1;
}
static inline int g28_clock_unpack(const uint8_t*b,size_t n,uint32_t*a){
 if(!b||!a||n!=G28_CLOCK_BYTES)return 0;for(unsigned i=0;i<20;i++){a[i]=0;for(unsigned j=0;j<4;j++)a[i]|=(uint32_t)b[4*i+j]<<(8*j);}
 return a[0]==0x314b4347u&&a[1]==1&&a[2]==G28_CLOCK_BYTES&&a[3]==g28_clock_crc(b+16,64)&&a[4]<=3&&a[5]<=63&&a[12]<=52&&a[13]<=174&&!a[18]&&!a[19];
}
