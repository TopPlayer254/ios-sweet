/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include "ziyi_g7_clock_baseline.h"
#include "ziyi_g7_clock_profile.h"
#define G2_RUN "ZIYI-20261026-G6"
enum {G2_BACKUP=1,G2_CX_ON=2,G2_CLOCKS=4,G2_RATES=8,G2_GMU_QUIET=16,G2_COUNTER=32,G2_SMMU_CAPTURED=64,G2_RESTORED=128};
enum {G2_E_IO=1,G2_E_OWNER,G2_E_BACKUP,G2_E_INITIAL,G2_E_POWER,G2_E_UPDATE,G2_E_CLOCK,G2_E_COUNTER,G2_E_RATE,G2_E_ACTIVE,G2_E_LAYOUT,G2_E_RESTORE,G2_E_EVENTS};
struct g7c_event {uint16_t index,phase;uint32_t value;};
struct g7c_record {
 struct g1_record base;
 uint32_t flags,phase,error,poison,restore,backup_crc,events,opaque;
 uint32_t old[G2_NREGS],expected[G2_NREGS],final[G2_NREGS];
 uint32_t gmuregs[8],measure[4][4],smmu[6],smr[128][2],cb[6][8];
 uint64_t gmu_counter[2],arm_counter[2];
 uint32_t smmu_page,smmu_cb_base,smmu_contexts,smmu_matches;
 uint32_t cx_on_status,cx_off_status,gx_before,gx_after;
 struct g7c_event trace[192];
};
_Static_assert(sizeof(struct g7c_record)==4752,"G2 wire ABI");
struct g7c_io {
 void *ctx;
 uint32_t (*read)(void *,uint64_t,uint32_t,int *);
 void (*write)(void *,uint64_t,uint32_t,uint32_t,int *);
 void (*delay)(void *,uint32_t);
 uint64_t (*ticks)(void *);
 void (*phase)(void *,uint32_t);
 int (*qualified)(void *,struct g7c_record *);
};
#ifdef __cplusplus
extern "C" {
#endif
int g7c_read_allowed(const struct g7c_record *,uint64_t,uint32_t);
int g7c_write_allowed(uint64_t,uint32_t);
int g7c_store_guard(const struct g7c_record *,uint64_t,uint32_t,uint32_t,uint32_t);
int g7c_run(struct g7c_record *,const struct g7c_io *);
uint32_t g7_clock_crc(const void *,size_t);
#ifdef __cplusplus
}
#endif
