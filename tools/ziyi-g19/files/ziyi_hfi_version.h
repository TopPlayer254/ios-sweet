/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include "ziyi_g8_contract.h"
#define GH_VERSION_COMMAND 1u
#define GH_A662_SUPPORTED_VERSION 0x20000000u
enum gh_error {GH_REMOTE_ERROR=100,GH_VERSION_MISMATCH};
struct gh_version_reply {uint32_t status,version,next_read;};
/* Pure wire contract only. Requires a separately qualified live NC queue owner;
 * no MMIO, cache mutation, doorbell, shared-index publication or GMU start. */
int gh_version_command(uint32_t sequence,uint32_t out[2]);
int gh_version_reply(const uint32_t ring[GN_HFI_WORDS],const struct gn_hfi_queue*,uint32_t sent,struct gh_version_reply*);
