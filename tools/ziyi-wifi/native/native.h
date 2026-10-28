/* SPDX-License-Identifier: GPL-2.0-only
 * Ziyi WPSS control transport. No automatic startup and no radio-ready flag.
 */
#pragma once
#include <stddef.h>
#include <stdint.h>
#define ZWN_BOARD 0x06060201u
#define ZWN_QMP 0x0c300000ULL
#define ZWN_QMP_BYTES 0x400u
#define ZWN_IPCC 0x0ed18000ULL
#define ZWN_IPCC_BYTES 0x1000u
#define ZWN_WPSS_IMAGE 0x85300000ULL
#define ZWN_WPSS_BYTES 0x01900000u
#define ZWN_PAS 6u
#define ZWN_UP 0x0000ffffu
#define ZWN_DOWN 0xffff0000u
enum zwn_error {ZWN_OK, ZWN_ARGUMENT, ZWN_IO, ZWN_OWNER, ZWN_FORMAT, ZWN_BUSY,
 ZWN_TIMEOUT, ZWN_SCM, ZWN_WAITQ, ZWN_INTERRUPTS, ZWN_SERVICE, ZWN_FATAL};
enum zwn_operation {ZWN_QMP_OPEN=1,ZWN_QMP_MESSAGE,ZWN_QMP_CLOSE,ZWN_PAS_PROBE,
 ZWN_PAS_INIT,ZWN_PAS_MEM,ZWN_PAS_START,ZWN_PAS_STOP,ZWN_SMP_STOP,ZWN_SMP_ACK};
/* authority must bind this exact board, epoch, DT/resources and operation;
 * it is repeated before every IO/SMC. A callback returning true is not itself
 * a native proof. The adapter must independently own and serialize resources. */
struct zwn_io {void *ctx; uint32_t (*read32)(void*,uint64_t,int*);
 void (*write32)(void*,uint64_t,uint32_t,int*);void (*barrier)(void*);
 void (*delay_us)(void*,uint32_t);void (*smc)(void*,uint64_t[8]);
 int (*authority)(void*,uint64_t,unsigned);};
struct zwn_qmp {uint64_t epoch;uint32_t error,poison,operation,reads,writes,kicks;
 uint32_t descriptor[15],original[4],expected[4],mailbox,size,open,inherited,
 changed,send_pending,sent,acknowledged,last_offset,last_value;};
/* open/send/close preserve an inherited open channel, reject in-flight data,
 * perform only aligned 32-bit QMP accesses and exact AOP doorbell0, and retain
 * uncertain ownership on IO/timeout instead of blindly undoing a store. */
int zwn_qmp_open(struct zwn_qmp*,uint64_t,const struct zwn_io*);
int zwn_qmp_send_image(struct zwn_qmp*,int on,const struct zwn_io*);
int zwn_qmp_close(struct zwn_qmp*,const struct zwn_io*);
int zwn_qmp_write_allowed(const struct zwn_qmp*,uint64_t,uint32_t,uint32_t);
struct zwn_scm {uint64_t epoch,last[8];uint32_t error,poison,calls,interrupts,
 operation,convention64,available,metadata_retained,started,stopped,
 initialized,memory_prepared,image_bytes;};
/* READONLY SCM availability calls precede PAS. No automatic retry of EBUSY
 * and no fake success for WAITQ; an unresolved secure session poisons state. */
int zwn_pas_probe(struct zwn_scm*,uint64_t,const struct zwn_io*);
/* The native authority for INIT must prove contiguous non-cacheable metadata,
 * no cached alias, exact file provenance and a fresh coldboot session. For
 * START it must additionally prove the image's actual stores/readback, QMP,
 * SMP2P and DMA mappings. This portable transport cannot establish those. */
int zwn_pas_init(struct zwn_scm*,uint64_t metadata_pa,uint32_t allocated_bytes,
 uint32_t metadata_bytes,const struct zwn_io*);
int zwn_pas_mem_setup(struct zwn_scm*,uint32_t image_bytes,const struct zwn_io*);
int zwn_pas_start(struct zwn_scm*,const struct zwn_io*);
int zwn_pas_shutdown(struct zwn_scm*,int fresh_stop_ack,const struct zwn_io*);
struct zwn_smp_view {uint32_t features,flags,valid,capacity,stop_offset,state,
 ready,fatal,handover,stop_ack;};
/* Parse a single already bounded allocated SMEM item, never arbitrary heaps.
 * expected_local/remote are exact0/13 or13/0, not any matching host tuple. */
int zwn_smp_parse(const uint8_t*,size_t,uint16_t expected_local,
 uint16_t expected_remote,const char*entry,struct zwn_smp_view*);
/* Change one owned master-kernel stop bit and preserve all other item bytes.
 * This helper prepares bytes only. Native publication requires APSS item
 * ownership and an independent locked read/compare/write then IPCC24/2. */
int zwn_smp_prepare_stop(const uint8_t*,size_t,uint8_t*,size_t,int request,
 struct zwn_smp_view*);
