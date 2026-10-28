/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <stdint.h>
#define GD_RESET 0x03d7e000ULL
#define GD_HALT 0x03d0f114ULL
#define GD_ACK 0x03d0f118ULL
struct gd_record {uint32_t phase,error,poison,client_ack,arb_ack,reads,stores,last_value;uint64_t epoch,last_address;};
struct gd_io {void*ctx;uint32_t(*read)(void*,uint64_t,int*);void(*write)(void*,uint64_t,uint32_t,int*);void(*delay)(void*,uint32_t);};
/* These are guarded IO algorithms, not native access callbacks. A future
 * transaction must provide owned source-bound mappings and a reset receipt.
 * No automatic unmap/free/TCM overwrite or HALT release is performed. */
int gd_store_guard(const struct gd_record *,uint64_t address,uint32_t value,uint32_t now);
int gd_drain(struct gd_record *,uint64_t same_session_epoch,unsigned timeout_us,const struct gd_io *);
