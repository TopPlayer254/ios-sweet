/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <stddef.h>
#include <stdint.h>
#define G24_SLOT_BYTES 65536u
#define G24_ZAP_PA 0x8bb1a000ULL
#define G24_ZAP_REGION_BYTES 8192u
#define G24_ZAP_LOAD_BYTES 4096u
struct g24_firmware {const uint8_t *sqe,*mdt,*code;uint32_t sqe_bytes,mdt_bytes,code_bytes,sqe_version,relocation,load_bytes;};
/* Generic parser takes explicit pins for CPU fixtures. Native slot access
 * binds the immutable own-device pins and never takes caller-supplied pins. */
int g24_capsule_view(const uint8_t*,size_t,const uint8_t [3][32],struct g24_firmware*);
int g24_mdt_plan(const uint8_t*,size_t,size_t,struct g24_firmware*);
int g24_slot_view(struct g24_firmware*);
