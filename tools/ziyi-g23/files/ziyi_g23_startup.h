/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include "ziyi_g23_resources.h"
#include "ziyi_g8_contract.h"
#define GS_COMMANDS 5u
#define GS_PACKET_WORDS 160u
#define GS_RECORD_BYTES 768u
enum {GS_NEW=0,GS_PREPARED=1,GS_RUNNING=2,GS_COMPLETE=3,GS_POISON=4};
enum {GS_OK=0,GS_ARGUMENT=300,GS_OWNER,GS_QUEUE,GS_IO,GS_TIMEOUT,GS_PACKET,GS_REMOTE,GS_CLOCK,GS_STATE};
enum {GS_ACQUIRE=1,GS_RELEASE=2};
struct gs_packet {uint32_t count,words[GS_PACKET_WORDS];};
struct gs_program {uint64_t epoch;uint32_t gmu_address,peak_kbps;struct gr_resources resources;struct gs_packet packet[GS_COMMANDS];};
struct gs_step {uint32_t header,crc,polls,stores,remote,raw_count,raw_declared,raw[19],q0_write,q1_read,q1_write,complete;};
struct gs_record {uint32_t state,error,current,calls,acks,bell_returned,resource_crc,header_crc,found,entries,peak_kbps,header_equal,resources_equal,read_words;uint32_t health[5],queue_key,queue_seen,queue_values[6];uint64_t epoch,start,end,frequency;struct gs_step step[GS_COMMANDS];};
struct gs_io {
 void*ctx;
 int(*qualified)(void*,uint64_t);
 int(*read32)(void*,uint32_t,uint32_t*);
 int(*write32)(void*,uint32_t,uint32_t);
 int(*order)(void*,unsigned);
 int(*doorbell)(void*,unsigned,const struct gs_packet*);
 int(*healthy)(void*);
 int(*now)(void*,uint64_t*);
 int(*delay)(void*,uint32_t);
};
uint32_t gs_crc(const uint8_t*,size_t);
int gs_prepare(struct gs_program*,const struct gr_resources*,uint64_t,uint32_t,uint32_t);
int gs_run(const struct gs_program*,const struct gs_io*,uint64_t,struct gs_record*);
int gs_encode(const struct gs_record*,uint8_t[GS_RECORD_BYTES]);
int gs_decode(const uint8_t*,size_t,struct gs_record*);
