/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <stdint.h>
#define ZWS_SMRS 120u
#define ZWS_CBS 78u
#define ZWS_CB_WORDS 11u
/* IHI0062D.c16.5.9: FSR[10:9] is read-only Format, not a fault flag.
 * Before ownership its format can be unknown. Once our VA64 controls are
 * qualified, require exactly AArch64 Format=2 and no other bit whatsoever.
 * No W1C status is cleared and no real/unknown fault bit is accepted. */
#define ZWS_FSR_FORMAT_MASK 0x600u
#define ZWS_FSR_AARCH64 0x400u
#define ZWS_FSR_FAULT_MASK 0xc00001feu
static inline int zws_fsr_clean(uint32_t raw,unsigned configured_aarch64)
{
 if(configured_aarch64>1)return 0;
 return configured_aarch64?raw==ZWS_FSR_AARCH64:(raw&~ZWS_FSR_FORMAT_MASK)==0;
}
/* A bounded HLOS snapshot, never the hardware's larger private register set. */
struct zws_snapshot {
 uint32_t scr0,idr0,idr1;
 uint32_t smr[ZWS_SMRS],s2cr[ZWS_SMRS],sctlr[ZWS_CBS];
};
struct zws_choice {uint32_t match,context,page,pages;};
/* Both passes must agree. Existing radio streams are not stolen; every live
 * translate context is reserved, including one outside the HLOS CB range.
 * An enabled but unreferenced CB is skipped rather than treated as free. */
int zws_select(const struct zws_snapshot*,const struct zws_snapshot*,struct zws_choice*);
struct u3_smmu;
int zws_storage_lease(unsigned match,unsigned context,uint32_t old_smr,uint32_t old_s2cr);
int zws_storage_coexists(const struct u3_smmu*,const struct u3_smmu*,const struct u3_smmu*);
uint64_t zws_storage_epoch(void);
uint64_t zws_storage_write_begin(void);
int zws_storage_write_end(uint64_t);
int zws_context_matches(const uint32_t actual[ZWS_CB_WORDS],const uint32_t wanted[ZWS_CB_WORDS]);
