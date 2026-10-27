/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <stdint.h>
/* Exact ziyi Diwali DT/vendor bindings. No Redwood address table. */
#define G2_GPUCC 0x03d90000ULL
#define G2_GCC 0x00100000ULL
#define G2_GMU 0x03d6a000ULL
#define G2_SMMU 0x03da0000ULL
#define G2_PWR_ON (1u<<31)
#define G2_COLLAPSE 1u
#define G2_HW_CONTROL 2u
#define G2_RETAIN (1u<<11)
#define G2_OFF (1u<<31)
#define G2_UPDATE 1u
#define G4_DIRTY_CFG (1u<<4) /* pinned clk-rcg2.c CMD_DIRTY_CFG; observation only */
#define G4_CMD_STATUS ((1u<<31)|G4_DIRTY_CFG)
#define G4_CMD_PENDING (G2_UPDATE|G4_DIRTY_CFG)
/* gdsc-regulator.c disable: >=8 XO cycles (1us), then no-status100us. */
#define G6_COLLAPSE_SETTLE_US 101u
#define G6_HLOS_CONTEXTS 6u
#define G6_HLOS_SMRS 6u
/* Set by the typed platform adapter only after its actual board/IO/execution
 * contract is verified. This is not an Android FDT result in kernel mode. */
#define G7C_PLATFORM_SCOPE (1u<<31)
#define G2_CFG_MASK 0x10371fu /* vendor plain RCG: explicitly clear HW_CLK_CTRL bit20 */
#define G2_MEASURE_RUN (1u<<20)
#define G2_MEASURE_CLEAR (1u<<21)
#define G2_MEASURE_DONE (1u<<25)
#define G2_MEASURE_MASK 0x3fffffu
#define G2_COUNTER_MASK 0x1ffffffu
#define G2_BRIEF 0x1000u
#define G2_FULL 0x20000u
enum g7c_reg_index {
 G2_CX_POWER,G2_PLL_VOTE,G2_GCC_MEMNOC,G2_GCC_SNOC,G2_GCC_DDR,
 G2_GMU_CFG,G2_GMU_CMD,G2_HUB_CFG,G2_HUB_CMD,
 G2_AHB,G2_CXO,G2_CXO_AON,G2_DEMET,G2_HUB_AON,G2_HUB_CX,G2_CX_GMU,G2_SMMU_VOTE,
 G2_GPU_MUX,G2_GPU_DIV,G2_GPU_DEBUG_CLK,G2_GCC_MUX,G2_GCC_DIV,G2_GCC_DEBUG_CLK,G2_XO4,G2_MEASURE_CTL,
 G2_NREGS
};
struct g7c_reg {uint64_t base;uint32_t offset,mask,ro_mask,opaque_mask;};
static const struct g7c_reg g7c_regs[G2_NREGS]={
 {G2_GPUCC,0x9108,0x801,0xf8000000,0},
 {G2_GCC,0x62000,0x10000,0,0},
 {G2_GCC,0x81010,1,0xf0000000,0},{G2_GCC,0x81018,1,0xf0000000,0},{G2_GCC,0x81154,1,0xf0000000,0},
 {G2_GPUCC,0x931c,G2_CFG_MASK,0,0},{G2_GPUCC,0x9318,1,G4_CMD_STATUS,0},
 {G2_GPUCC,0x93f0,G2_CFG_MASK,0,0},{G2_GPUCC,0x93ec,1,G4_CMD_STATUS,0},
 {G2_GPUCC,0x911c,1,0xf0000000,1}, /* documented HALT_SKIP/readback quirk; rate proof required */
 {G2_GPUCC,0x9144,1,0xf0000000,0},{G2_GPUCC,0x9004,1,0xf0000000,0},{G2_GPUCC,0x900c,1,0xf0000000,0},
 {G2_GPUCC,0x93e8,1,0xf0000000,0},{G2_GPUCC,0x9148,1,0xf0000000,0},
 {G2_GPUCC,0x913c,0xf1,0xf0000000,0}, /* Diwali sleep-staggering workaround remains until CX is off */
 {G2_GPUCC,0x7000,1,0xf0000000,0},
 {G2_GPUCC,0x9564,0xff,0,0},{G2_GPUCC,0x9270,0xf,0,0},{G2_GPUCC,0x9274,1,0xf0000000,0},
 {G2_GCC,0x72000,0x3ff,0,0},{G2_GCC,0x72004,0xf,0,0},{G2_GCC,0x72008,1,0xf0000000,0},
 {G2_GCC,0x7200c,1,0xf0000000,0},{G2_GCC,0x72038,G2_MEASURE_MASK,0,0}
};
#define G2_GCC_MEASURE_STATUS 0x7203cu
#define G2_CX_HW_STATUS 0x953cu
#define G2_GX_POWER 0x905cu
#define G2_GCC_GPU_SELECT 0x16eu
static const uint32_t g7c_clock_selects[4]={0x1f,0x16,0x1e,0x2f}; /* XO, AHB, GMU, direct HUB_AON (not divided HUB_CX_INT) */
static const uint32_t g7c_gmu_offsets[8]={0x14000,0x14004,0x14070,0x140b4,0x14600,0x14604,0x14340,0x143b0};

/* G2B native CRC22d0054c: CX upper observation states 0 / f8000000.
 * Names and RO semantics of bits30:27 are not inferred. Never changed by us. */
#define G3_CX_OBSERVED_UPPER 0xf8000000u
