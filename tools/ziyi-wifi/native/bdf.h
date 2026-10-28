/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <stddef.h>
#include <stdint.h>
/* Xiaomi icnss2/qmi.c ELF naming, after a successful target-capabilities reply.
 * country_global must come from the bound hardware region, not iOS language.
 * Foundry affects BIN naming only; this path deliberately requests ELF. */
int zwn_bdf_elf(uint32_t board_id,int project_l9s,int country_global,char*,size_t);
