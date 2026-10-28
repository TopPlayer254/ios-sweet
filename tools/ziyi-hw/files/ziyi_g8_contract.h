/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <stddef.h>
#include <stdint.h>

/* Pure parsers/builders. This interface has NO MMIO, SMC, DMA or file access. */
#define GN_PAGE 4096u
#define GN_MAX_BLOCKS 64u
#define GN_MAX_REGIONS 16u
#define GN_HFI_QUEUES 3u
#define GN_HFI_WORDS 1024u
#define GN_HFI_BYTES 16384u
#define GN_HFI_TABLE_BYTES 840u
#define GN_HFI_QUEUE0 848u
enum gn_error {GN_OK=0,GN_ARG,GN_TRUNCATED,GN_LIMIT,GN_ADDRESS,GN_METADATA,
 GN_OVERLAP,GN_BACKING,GN_QUEUE,GN_FULL,GN_EMPTY,GN_PACKET,GN_STALE,GN_WRAP};
enum gn_region_kind {GN_ITCM=1,GN_DTCM,GN_CACHE,GN_NC,GN_LOG,GN_HFI};
struct gn_block {uint32_t address,size,type,value,data_offset,kind;};
struct gn_region {uint32_t address,size,kind;};
struct gn_plan {
 uint32_t block_count,region_count,data_blocks,metadata_blocks;
 uint32_t backing_bytes,log_address,hfi_address;
 struct gn_block blocks[GN_MAX_BLOCKS];
 struct gn_region regions[GN_MAX_REGIONS];
};
struct gn_hfi_queue {
 uint32_t status,start,type,words,message_words,unused[5],read,write;
};
struct gn_hfi_table {
 uint32_t version,words,first_queue,queue_words,queues,active_queues;
 struct gn_hfi_queue queue[17];
};
_Static_assert(sizeof(struct gn_hfi_queue)==48,"vendor HFI queue ABI");
_Static_assert(sizeof(struct gn_hfi_table)==GN_HFI_TABLE_BYTES,"vendor HFI table ABI");
int gn_firmware_plan(const void *data,size_t bytes,struct gn_plan *out);
int gn_hfi_init(void *memory,size_t bytes,uint32_t gmu_address);
int gn_hfi_sequence(uint32_t *sequence,uint32_t *next);
int gn_hfi_header(uint32_t id,uint32_t words,uint32_t sequence,uint32_t *header);
/* HFI v2 only. The caller must later publish writes with release/cache maintenance before
 * a hardware doorbell. Builders never publish a doorbell or shared index. */
int gn_hfi_enqueue(uint32_t ring[GN_HFI_WORDS],const struct gn_hfi_queue *queue,
 const uint32_t *message,uint32_t words,uint32_t *new_write);
int gn_hfi_ack(const uint32_t ring[GN_HFI_WORDS],const struct gn_hfi_queue *queue,
 uint32_t sent_header,uint32_t *verdict,uint32_t *new_read);
