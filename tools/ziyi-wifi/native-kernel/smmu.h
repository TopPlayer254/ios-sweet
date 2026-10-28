/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <stdint.h>
#define ZWS_SMRS 120u
#define ZWS_CBS 78u
#define ZWS_CB_WORDS 11u
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
