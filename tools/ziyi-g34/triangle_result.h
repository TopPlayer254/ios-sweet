/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <stddef.h>
#include <stdint.h>
enum g34_stage { G34_NEW,G34_BASELINE,G34_MEMORY,G34_PROGRAM,G34_READY,
 G34_SUBMITTED,G34_WAITING,G34_PIXELS,G34_PRESENTED,G34_DONE,G34_REFUSED=15 };
enum g34_error { G34_OK,G34_ARGUMENT,G34_OWNER,G34_BASELINE_BAD,G34_ASSET_BAD,
 G34_IO,G34_ACTIVE,G34_FAULT,G34_TIMEOUT,G34_FENCE_BAD,G34_PIXEL_BAD,G34_GUARD_BAD,G34_DISPLAY_BAD };
enum {G34_PROOF_OWNER=1,G34_PROOF_PROGRAM=2,G34_PROOF_SHADERS=4,
 G34_PROOF_SUBMIT=8,G34_PROOF_CONSUMED=16,G34_PROOF_FENCES=32,
 G34_PROOF_PIXELS=64,G34_PROOF_GUARDS=128,G34_PROOF_PRESENTED=256,G34_PROOF_ALL=511};
struct g34_result {
 uint64_t epoch,start,end;
 uint32_t stage,failure_stage,error,proofs;
 uint32_t ring_begin,ring_end,rptr,wptr,status,intr,cp_fault,cp_protect,smmu_fsr,global_fsr;
 uint32_t observed[2],tokens[2],vs_crc,fs_crc,program_crc,pixel_crc;
 uint32_t triangle_pixels,background_pixels,first_bad_byte,shader_bytes;
 uint8_t source_identity[32],asset_identity[32];
 uint32_t owner_flags,guard_crc,guard_count,vertex_count,fragment_outputs,retained;
};
int g34_triangle_covered(unsigned,unsigned);
int g34_check_pixels(struct g34_result*,const volatile uint8_t*,size_t);
int g34_result_pack(const struct g34_result*,uint8_t*,size_t);
int g34_result_valid(const uint8_t*,size_t);
