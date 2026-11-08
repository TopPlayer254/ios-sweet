/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_g24_firmware.h"
#include "ziyi_g7_sha256.h"
#include <string.h>
static uint32_t g24f_u32(const uint8_t*p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint32_t g24f_crc(const uint8_t*p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;i++){c^=p[i];for(unsigned b=0;b<8;b++)c=(c>>1)^(0xedb88320u&(0u-(c&1u)));}return ~c;}
int g24_mdt_plan(const uint8_t*b,size_t n,size_t code_bytes,struct g24_firmware*out){
 if(!b||!out||n!=3564||code_bytes!=1976||memcmp(b,"\177ELF\1\1\1",7)||b[16]!=2||b[17]||b[18]!=164||b[19])return 0;
 /* Own MDT: three ELF32 program headers; b01 is executable and b02 hashes.
  * A650's b02-as-code recipe is expressly not accepted. */
 if(g24f_u32(b+28)!=52||b[40]!=52||b[41]||b[42]!=32||b[43]||b[44]!=3||b[45])return 0;
 static const uint32_t expected[3][8]={{0,0,0,0,148,0,0x07000000,0},{1,0x1000,0x1000,0x1000,1976,1976,0x08000007,0x100000},{0,0x2000,0,0,3416,3416,0x02000000,0x1000}};
 for(unsigned i=0;i<3;i++)for(unsigned j=0;j<8;j++)if(g24f_u32(b+52+32*i+4*j)!=expected[i][j])return 0;
 if(148u+3416u!=n)return 0;
 out->relocation=0x1000;out->load_bytes=4096;return 1;
}
int g24_capsule_view(const uint8_t*b,size_t n,const uint8_t pins[3][32],struct g24_firmware*out){
 static const uint8_t magic[16]={'Z','I','Y','I','_','G','2','4','_','C','P','_','F','W','0','1'};
 if(!b||!pins||!out||n!=G24_SLOT_BYTES||memcmp(b,magic,16)||g24f_u32(b+16)!=1||g24f_u32(b+20)!=n||g24f_u32(b+24)!=0x06060201||g24f_u32(b+28)!=1||g24f_u32(b+252)!=g24f_crc(b,252))return 0;
 const uint32_t starts[3]={256,43328,46896},sizes[3]={43068,3564,1976};uint8_t hash[32];
 for(unsigned i=0;i<3;i++){
  if(g24f_u32(b+32+8*i)!=starts[i]||g24f_u32(b+36+8*i)!=sizes[i]||memcmp(b+64+32*i,pins[i],32))return 0;
  g7_sha256(b+starts[i],sizes[i],hash);if(memcmp(hash,pins[i],32))return 0;
 }
 if(g24f_u32(b+56)!=4096||g24f_u32(b+60)!=4096)return 0;
 for(unsigned i=192;i<252;i++)if(b[i])return 0;
 for(unsigned i=256;i<n;i++){
  int member=0;for(unsigned j=0;j<3;j++)member|=i>=starts[j]&&i-starts[j]<sizes[j];if(!member&&b[i])return 0;
 }
 g7_sha256(b+256,n-256,hash);if(memcmp(hash,b+160,32))return 0;
 struct g24_firmware f={.sqe=b+starts[0],.mdt=b+starts[1],.code=b+starts[2],.sqe_bytes=sizes[0],.mdt_bytes=sizes[1],.code_bytes=sizes[2]};
 if(g24f_u32(f.sqe)!=0||!g24_mdt_plan(f.mdt,f.mdt_bytes,f.code_bytes,&f))return 0;
 f.sqe_version=g24f_u32(f.sqe+4);if(f.sqe_version!=0x016dc109)return 0;
 *out=f;return 1;
}
#if defined(__APPLE__)
#define G24S_ATTR __attribute__((section("__DATA_CONST,__ziyi_g24"),used,aligned(16384)))
#else
#define G24S_ATTR __attribute__((used,aligned(64)))
#endif
static const uint8_t g24_private_slot[G24_SLOT_BYTES] G24S_ATTR={'Z','I','Y','I','_','G','2','4','_','C','P','_','F','W','0','1'};
#include "ziyi_g24_pins.inc"
__attribute__((noinline)) int g24_slot_view(struct g24_firmware*out){return g24_capsule_view(g24_private_slot,sizeof(g24_private_slot),g24_own_sha,out);}
