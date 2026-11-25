/* SPDX-License-Identifier: GPL-2.0-only
 * Fabricated CPU fixture only; never used to draw on the phone.
 */
#include "triangle_result.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
 uint8_t pixels[16384],record[256];unsigned count=0;
 /* Independent row-span oracle, rather than the implementation's edges. */
 for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++){
  int interior=y>=16&&y<48&&(int)(4*x)>=31+2*(int)y&&(int)(4*x)<=221-2*(int)y;
  count+=(unsigned)interior;
  uint32_t c=interior?0xffff00ffu:y<32?0xffff8000u:0xff0080ffu;
  for(unsigned b=0;b<4;b++)pixels[(y*64+x)*4+b]=(uint8_t)(c>>(8*b));
 }
 assert(count==512);
 struct g34_result r={.epoch=1,.stage=G34_PIXELS};assert(g34_check_pixels(&r,pixels,sizeof(pixels)));
 assert(r.triangle_pixels==512&&r.background_pixels==3584&&r.first_bad_byte==UINT32_MAX);
 r.stage=G34_DONE;r.proofs=511;r.retained=1;r.vertex_count=3;r.fragment_outputs=1;
 r.ring_begin=103;r.ring_end=r.rptr=r.wptr=303;
 r.observed[0]=r.tokens[0]=1;r.observed[1]=r.tokens[1]=2;
 r.vs_crc=1;r.fs_crc=2;r.program_crc=3;r.shader_bytes=640;r.owner_flags=15;r.guard_count=32756;
 assert(g34_result_pack(&r,record,sizeof(record)));
 for(unsigned bit=0;bit<2048;bit++){record[bit/8]^=(uint8_t)(1u<<(bit%8));assert(!g34_result_valid(record,sizeof(record)));record[bit/8]^=(uint8_t)(1u<<(bit%8));}
 for(unsigned i=0;i<16384;i+=31){pixels[i]^=1;struct g34_result f={.epoch=1,.stage=G34_PIXELS};assert(!g34_check_pixels(&f,pixels,sizeof(pixels)));assert(f.first_bad_byte==i);pixels[i]^=1;}
 puts("G34_TRIANGLE_CHECKER_PASS synthetic only: independent 512-pixel oracle, 529 readback corruptions, 2048 record-bit refusals");
 return 0;
}
