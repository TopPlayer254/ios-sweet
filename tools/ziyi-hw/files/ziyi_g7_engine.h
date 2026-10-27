/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "ziyi_g7_policy.h"
#include "ziyi_g7_controls.h"
#define G7_TCM_BYTES 32768u
#define G7_CB_WORDS 15u
#define G7_BOARD 0x06060201u
enum g7_error {G7_OK=0,G7_ARG,G7_SCOPE,G7_LEASE,G7_PREREQ,G7_IO,G7_CHANGED,G7_OWNER,G7_BUFFER,G7_ACTIVE,G7_NOT_READY};
enum g7_flags {G7_LOCKED=1,G7_QUIET=2,G7_SNAPSHOT=4,G7_PLAN=8,G7_CONTEXT=16,G7_CONTROLS_SAVED=32,G7_TCM_SAVED=64,G7_FINAL_QUIET=128,G7_RELEASED=256};
enum g7_contract_kind {G7_CONTRACT_LIVE_FDT=1,G7_CONTRACT_XNU_SECURE_DT=2};
enum g7_execution_kind {G7_EXEC_FLAT_OFF=1,G7_EXEC_KERNEL_ON=2};
/* Same-session proof from the actual open G6 qualifier or equivalent XNU
 * adapter. It is not a hardware ownership grant. Adapter audit must prove
 * board/FDT, device mapping, exclusive manager and current clock epoch. */
struct g7_prereq {uint64_t epoch;uint32_t board,hlos_cb,hlos_smr,contract_kind,handoff_pairs,rates[4],mapped_device_io,execution_kind,current_el;uint64_t sctlr;};
struct g7_buffers {uint8_t *copy0,*copy1;size_t bytes0,bytes1;};
struct g7_io {
 void *ctx;
 uint32_t (*read)(void *,uint64_t,int *);
 uint64_t (*ticks)(void *);
 int (*lease)(void *,uint64_t,int); /* acquire1/release0, exclusive manager */
 int (*owned_buffer)(void *,const void *,size_t); /* actual allocator/BSS */
 void (*barrier)(void *);
};
struct g7_record {
 uint32_t version,bytes,flags,phase,error,poison,reads,lease_released;
 uint64_t epoch,start,end,last_address;uint32_t last_value,reserved;
 struct ga_snapshot snapshot;struct ga_plan plan;
 uint32_t context[2][G7_CB_WORDS],gr1[2][2],controls[2][G7_CONTROLS];
 uint32_t tcm_crc[2],tcm_equal,device_writes;
 uint8_t tcm_sha256[2][32];
};
/* Native/XNU adapters MUST apply this gate immediately before each read.
 * No write callback exists: this complete acquisition/backup engine grants
 * no SMMU/TCM/config/firmware write or firmware-execution authority. */
#ifdef __cplusplus
extern "C" {
#endif
int g7_read_allowed(const struct g7_record *,uint64_t);
int g7_run_acquire(struct g7_record *,const struct g7_prereq *,const struct g7_buffers *,const struct g7_io *);
/* Full FW_INIT100 remains denied until register-write and bus-drain/restore
 * contracts are implemented/audited. An acquisition pass cannot start it. */
int g7_firmware_ready(const struct g7_record *);

#ifdef __cplusplus
}
#endif
