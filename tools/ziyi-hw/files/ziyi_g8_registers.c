/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_g8_fwinit.h"
/* Full literals are FINITE allowed values, never arbitrary writable masks.
 * ARCH masks come from named ARM-SMMU fields, not vendor constant writes. */
#define L(a,v) {a,0,v,4,G8_LITERAL}
#define A(a,m,v,w) {a,m,v,w,G8_ARCH}
const struct g8_reg g8_regs[G8_REGS]={
 L(0x3d7e3c4,1),L(0x3d7e3c0,1),L(0x3d7e730,1),
 {0x3d7e070,0,0,4,G8_FWSTATUS},L(0x3d7e004,2),
 L(0x3d7e614,0),L(0x3d7e610,1),L(0x3d8ec44,0x80a008a0),L(0x3d7e0b4,0x4052),
 L(0x3d7e404,0),L(0x3d7e608,0x06062100),L(0x3d7e400,0),
 L(0x3d7d03c,1),L(0x3d7d000,1),L(0x3d7d004,1),
 {0x3d7e300,7,0x9c40400,4,G8_INTERFRAME},L(0x3d7e304,0xa1680),L(0x3d7e308,0xa1680),
 {0x3d7e3a0,0xf11,0xf11,4,G8_MASK},
 {0x3d7e000,1,0,4,G8_RESET},L(0x3d7e64c,0xffffffff),L(0x3d8ec18,0xffffffff),L(0x3d8ec40,0),
 {0x3d0f114,3,0,4,G8_GBIF},
 A(0x3da1008,0x3f3ff,0x1f300,4),A(0x3da1808,1,1,4),
 A(0x3db2000,0x0fdf11f7,0xc01027,4),L(0x3db2004,0x32b),
 A(0x3db2010,0x3801f,0x38011,4),A(0x3db2030,0x80c0ffbf,0x803519,4),
 A(0x3db2020,0xffff000ffffff000ULL,0,8),A(0x3db2028,0xffff000ffffff000ULL,0x2000000000000ULL,8),
 A(0x3db2038,0xffffffff,0xf404ff44,4),A(0x3db203c,0xffffffff,0xefe4,4),
 {0x3da0800,0,0x84000005,4,G8_ROUTE},{0x3da0c00,0,0x302,4,G8_ROUTE},
 A(0x3db2058,0xc00001fe,0,4)
};
