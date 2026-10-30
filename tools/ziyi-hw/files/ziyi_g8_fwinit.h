/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include "ziyi_g8_slot.h"
#include "ziyi_g7_policy.h"
#include "ziyi_g8_memory.h"
#include "ziyi_g8_drain.h"
#define G8_REGS 37u
#define G8_TCM_BYTES 32768u
enum g8_phase {G8_ACQUIRE=1,G8_SAVE,G8_MEMORY,G8_INSTALL,G8_LOAD,G8_CONFIG,G8_START,G8_WAIT,G8_STOP,G8_DRAIN,G8_CLEAR,G8_UNROUTE,G8_CONTEXT_RESTORE,G8_TCM_RESTORE,G8_CONFIG_RESTORE,G8_DONE,G9_RELEASE_BUS};
enum g8_flags {G8_LEASE=1,G8_SAVED=2,G8_MEMORY_READY=4,G8_PUBLISHED=8,G8_MAPPED=16,G8_LOADED=32,G8_RELEASED_RESET=64,G8_FW100=128,G8_HELD_RESET=256,G8_BUS_DRAINED=512,G8_RESTORED=1024,G8_LEASE_RELEASED=2048,G8_SCOPED=4096};
enum g8_error {G8_OK=0,G8_ARGUMENT,G8_PREREQ,G8_IO,G8_OWNER,G8_BACKUP,G8_MEMORY_ERROR,G8_FIRMWARE,G8_TIMEOUT,G8_DEVICE_FAULT,G8_RESTORE};
enum g8_kind {G8_LITERAL=1,G8_MASK,G8_INTERFRAME,G8_RESET,G8_FWSTATUS,G8_ARCH,G8_ROUTE,G8_GBIF};
struct g8_reg {uint64_t address,mask,literal;uint32_t width,kind;};
extern const struct g8_reg g8_regs[G8_REGS];
struct g8_prereq {uint64_t epoch;uint32_t board,hlos_cb,hlos_smr,rates[4],current_el;uint64_t sctlr;uint32_t acd_off,qdss_off,lm_bcl_off,hfi_off,inherited_ddr_probe;};
struct g8_record {uint32_t flags,phase,error,poison,reads,writes,tcmmodified,restored,lease_released,retain_allocations,pending_width,tcm_cursor,saved_crc;uint64_t epoch,start,end,pending_address,pending_value,last_address,last_value,old[G8_REGS],expected[G8_REGS];uint32_t pending_clear,last_fsr,last_host,last_ao,last_fence;uint8_t tcm_sha[2][32];struct gd_record drain;uint32_t initial_halt,initial_ack,final_ack,bus_released,bus_observations;};
struct g8_workspace {struct g8_record record;struct ga_snapshot snapshot;struct ga_plan lease_plan;struct gn_plan firmware_plan;struct g8_blob blob;uint8_t firmware[G8_FW_BYTES] __attribute__((aligned(4096)));uint8_t backup[2][G8_TCM_BYTES] __attribute__((aligned(4096)));struct gm_xnu_memory memory;};
struct g8_io {void*ctx;uint64_t(*read)(void*,uint64_t,unsigned,int*);void(*write)(void*,uint64_t,unsigned,uint64_t,int*);uint64_t(*ticks)(void*);void(*delay)(void*,uint32_t);void(*barrier)(void*);int(*lease)(void*,uint64_t,int);int(*proof)(void*,const struct g8_prereq*,const struct g8_blob*);int(*owned)(void*,const void*,size_t);struct gm_memory_io memory_io;};
/* Native callback MUST use observation_address then repeat store_guard with
 * a fresh reset value immediately before actual 32/64-bit device store. */
int g8_observation_address(uint64_t,unsigned,uint64_t*);
int g8_read_allowed(const struct g8_workspace*,uint64_t,unsigned);
int g8_store_guard(const struct g8_workspace*,uint64_t,unsigned,uint64_t,uint64_t,uint32_t,uint32_t);
int g8_run(struct g8_workspace*,const struct g8_prereq*,const struct g8_io*);
int g8_transaction_complete(const struct g8_workspace*);
/* Structural selection for the proven EL1/MMU-on contract. No legacy offline
 * GA_REQUIRED_FACTS assertion and no hardware write authority is returned. */
int g8_kernel_plan(const struct g8_prereq*,const struct ga_snapshot*,struct ga_plan*);
