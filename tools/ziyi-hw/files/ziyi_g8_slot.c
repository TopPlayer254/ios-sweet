/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_g8_slot.h"
#include "ziyi_g7_sha256.h"
#if defined(__APPLE__)
#define SLOT_ATTR __attribute__((section("__DATA_CONST,__ziyi_gmu"),used,aligned(16384)))
#else
#define SLOT_ATTR __attribute__((used,aligned(4096)))
#endif
/* Nonzero public header makes this file-backed, not zero-fill BSS. No vendor
 * bytes or private digest exist in CI. Root patches only this section locally. */
static const volatile uint8_t g8_private_slot[G8_SLOT_BYTES] SLOT_ATTR={
 'Z','I','Y','I','_','G','M','U','_','S','L','O','T','v','1',0,
 [16]=1,[22]=1,[24]=128,[28]=1,[29]=2,[30]=6,[31]=6
};
static uint32_t word(const uint8_t*p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int same(const uint8_t*a,const uint8_t*b,unsigned n){unsigned x=0;for(unsigned i=0;i<n;i++)x|=a[i]^b[i];return !x;}
static int overlaps(const void*a,size_t an,const void*b,size_t bn){uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;if(an>UINTPTR_MAX-x||bn>UINTPTR_MAX-y)return 1;return x<y+bn&&y<x+an;}
int g8_slot_decode(const volatile uint8_t*slot,size_t count,uint8_t*owned,size_t capacity,struct g8_blob*out){
 static const uint8_t magic[16]={'Z','I','Y','I','_','G','M','U','_','S','L','O','T','v','1',0};
 if(!slot||!owned||!out||count!=G8_SLOT_BYTES||capacity<G8_FW_BYTES||overlaps((const void*)slot,count,owned,G8_FW_BYTES)||overlaps((const void*)slot,count,out,sizeof(*out))||overlaps(owned,G8_FW_BYTES,out,sizeof(*out)))return 0;
 *out=(struct g8_blob){0};uint8_t h[128],header_hash[32],firmware_hash[32];
 for(unsigned i=0;i<128;i++)h[i]=slot[i];
 if(!same(h,magic,16)||word(h+16)!=1||word(h+20)!=65536||word(h+24)!=128||word(h+28)!=0x06060201||word(h+32)!=G8_FW_BYTES||word(h+36)!=1)return 0;
 for(unsigned i=104;i<128;i++)if(h[i])return 0;
 for(unsigned i=0;i<32;i++){out->firmware_sha256[i]=h[40+i];out->header_sha256[i]=h[72+i];h[72+i]=0;}
 g7_sha256(h,128,header_hash);if(!same(header_hash,out->header_sha256,32))return 0;
 for(unsigned i=0;i<G8_FW_BYTES;i++)owned[i]=slot[128+i];
 for(unsigned i=128+G8_FW_BYTES;i<G8_SLOT_BYTES;i++)if(slot[i])return 0;
 g7_sha256(owned,G8_FW_BYTES,firmware_hash);if(!same(firmware_hash,out->firmware_sha256,32))return 0;
 for(unsigned i=0;i<128;i++)if((i>=72&&i<104?out->header_sha256[i-72]:h[i])!=slot[i])return 0;
 out->bytes=G8_FW_BYTES;out->board=0x06060201;return 1;
}
int g8_slot_copy(uint8_t*owned,size_t capacity,struct g8_blob*out){return g8_slot_decode(g8_private_slot,sizeof(g8_private_slot),owned,capacity,out);}
