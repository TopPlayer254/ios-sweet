/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <stddef.h>
#include <stdint.h>
#define GP_PAGE 16384u
#define GP_WIDTH 64u
#define GP_HEIGHT 64u
#define GP_PITCH 256u
#define GP_WORDS 87u
enum { GP_OK=0,GP_BAD_ARGUMENT=1,GP_OWNERSHIP=2,GP_STALE=3,GP_FAULT=4,GP_PIXEL=5,GP_GUARD=6 };
/* These are CPU-side bounds, not proof that native mappings exist. Native
 * translation, ownership and cache visibility must be established separately. */
struct gp_page { uint32_t iova,bytes,epoch,private_owned; };
struct gp_request {
 uint32_t epoch,color_token,flush_token;
 struct gp_page ring,target,fence;
};
enum {
 GP_NATIVE_HFI=1u,GP_NATIVE_GX_POWER=2u,GP_NATIVE_SMMU=4u,
 GP_NATIVE_SQE=8u,GP_NATIVE_ZAP=16u,GP_NATIVE_CP_FENCE=32u,
 GP_NATIVE_VISIBILITY=64u,GP_NATIVE_ALL=127u
};
struct gp_observation {
 uint32_t epoch,proof_flags,rptr,wptr,color_token,flush_token;
 uint32_t cp_fault,smmu_fault,gpu_busy;
};
int gp_build(uint32_t *out,size_t capacity,const struct gp_request *request,size_t *words);
int gp_submit_allowed(const struct gp_request *request,const struct gp_observation *observation);
int gp_verify(const struct gp_request *request,const struct gp_observation *observation,
 const volatile uint8_t *pixels,size_t bytes,const uint8_t *guard_before,
 const volatile uint8_t *guard_after,size_t guard_bytes,uint32_t *first_bad_byte);
