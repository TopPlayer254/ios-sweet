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
struct g24_workspace {
 struct g24_record r;struct g24_firmware fw;struct gc_plan memory;struct g24_pas pas;
 volatile uint32_t *ring,*fence;volatile uint8_t*pixels;
 struct gp_request pixel_request;
 uint8_t guard[64];uint32_t expected_ring[256];
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
