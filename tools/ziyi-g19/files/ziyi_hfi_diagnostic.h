/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include "ziyi_hfi_live.h"

#define G19_HFI_MAX_POLLS 1000000u
#define G19_HFI_RECORD_BYTES 320u
#define G19_HFI_RECORD_MAGIC 0x31464847u /* GHF1, little endian */

enum g19_hfi_stage {
 G19_HFI_VALIDATE=1,G19_HFI_OWNER,G19_HFI_PREHEALTH,G19_HFI_INITIALIZE,
 G19_HFI_READY,G19_HFI_PUBLISH,G19_HFI_ACK,G19_HFI_COMPLETE
};
enum g19_hfi_error {
 G19_HFI_ARGUMENT=200,G19_HFI_CLOCK,G19_HFI_TIMEOUT,G19_HFI_POLL_CAP,
 G19_HFI_HEALTH,G19_HFI_START,G19_HFI_OWNERSHIP,G19_HFI_DELAY,
 G19_HFI_SNAPSHOT,G19_HFI_RECORD
};
enum g19_hfi_flags {
 G19_HFI_PLANNED=1u,G19_HFI_START_CALLED=2u,G19_HFI_START_RETURNED=4u,
 G19_HFI_PUBLISHED=8u,G19_HFI_BELL_CALLED=16u,G19_HFI_CONSUMED=32u,
 G19_HFI_FIRST_HEALTH=64u,G19_HFI_LAST_HEALTH=128u,
 G19_HFI_RETAIN=256u,G19_HFI_TICKS=512u
};
struct g19_hfi_health {
 uint32_t fw_result,hfi_status,reset,helper_phase,helper_error;
};
struct g19_hfi_config {
 uint64_t epoch,timeout_ticks;
 uint32_t gmu_address,sequence,poll_cap,delay_us;
};
/* Every callback returns 1 on success, 0 on refusal/failure. It must itself
 * finish in bounded time. ticks are monotonic wall-clock ticks; caller limits
 * timeout_ticks to at most five seconds in that clock's units. health must
 * reject unknown hardware bits using the native source contract, while still
 * filling any observed values/helper error. No post-FW100 status-zero demand.
 * start_hfi performs exactly the source-qualified HFI_CTRL_INIT=1 operation;
 * this module never resets, stops, remaps, frees or retries a command. */
struct g19_hfi_ops {
 void *ctx;
 struct gh_live_io live;
 int (*health)(void *,struct g19_hfi_health *);
 int (*start_hfi)(void *);
 int (*ticks)(void *,uint64_t *);
 int (*delay)(void *,uint32_t us);
};
struct g19_hfi_record {
 uint32_t stage,error,flags,live_state,sequence,sent[2];
 uint32_t poll_cap,polls,ready_polls,ack_polls,remote_status,version;
 uint32_t raw_declared,raw_count,owner_failed,first_mask,last_mask;
 uint32_t queue_first[4],queue_last[4]; /* q0 read/write, q1 read/write */
 uint32_t raw[19]; /* prefix actually read by gh_live, including malformed */
 struct g19_hfi_health first_health,last_health;
 uint32_t gmu_address,stores,q0_write_stores,q1_read_stores;
 uint64_t epoch,start_tick,last_tick,timeout_ticks;
 uint32_t delay_us,start_calls,doorbells;
};
/* Caller-owned storage: do not put the 4 KiB gh_live scratch on a kernel stack.
 * One serialized invocation per retained lease. Reentry is refused. */
struct g19_hfi_runtime {
 struct gh_live live;
 const struct g19_hfi_ops *ops;
 struct g19_hfi_record *record;
 uint32_t attempted,capture_reply,reply_read;
};
int g19_hfi_run(struct g19_hfi_runtime *,const struct g19_hfi_config *,
 const struct g19_hfi_ops *,struct g19_hfi_record *);
uint32_t g19_hfi_crc32(const uint8_t *,size_t);
int g19_hfi_record_encode(const struct g19_hfi_record *,uint8_t out[G19_HFI_RECORD_BYTES]);
int g19_hfi_record_decode(const uint8_t *,size_t,struct g19_hfi_record *);
