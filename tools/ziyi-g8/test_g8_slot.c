/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_g8_slot.h"
#include "ziyi_g7_sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t input[G8_SLOT_BYTES],valid[G8_SLOT_BYTES],owned[G8_FW_BYTES];
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"slot line%u\n",__LINE__);exit(1);}}while(0)
static void u32(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
int main(void){struct g8_blob b;memcpy(valid,"ZIYI_GMU_SLOTv1",15);u32(valid+16,1);u32(valid+20,65536);u32(valid+24,128);u32(valid+28,0x06060201);u32(valid+32,G8_FW_BYTES);u32(valid+36,1);for(unsigned i=0;i<G8_FW_BYTES;i++)valid[128+i]=(uint8_t)(i*17+6);g7_sha256(valid+128,G8_FW_BYTES,valid+40);g7_sha256(valid,128,valid+72);memcpy(input,valid,sizeof(input));CHECK(g8_slot_decode(input,sizeof(input),owned,sizeof(owned),&b));CHECK(b.bytes==G8_FW_BYTES&&b.board==0x06060201&&!memcmp(owned,valid+128,sizeof(owned)));CHECK(!g8_slot_copy(owned,sizeof(owned),&b));
 for(unsigned i=0;i<G8_SLOT_BYTES;i+=i<128?1:997){memcpy(input,valid,sizeof(input));input[i]^=1;CHECK(!g8_slot_decode(input,sizeof(input),owned,sizeof(owned),&b));}
 memcpy(input,valid,sizeof(input));CHECK(!g8_slot_decode(input,sizeof(input)-1,owned,sizeof(owned),&b));CHECK(!g8_slot_decode(input,sizeof(input),owned,sizeof(owned)-1,&b));CHECK(!g8_slot_decode(input,sizeof(input),input+128,G8_FW_BYTES,&b));CHECK(!g8_slot_decode(input,sizeof(input),owned,sizeof(owned),(struct g8_blob*)(void*)(owned+20)));CHECK(!g8_slot_decode(input,sizeof(input),owned,sizeof(owned),(struct g8_blob*)(void*)(input+40)));CHECK(!g8_slot_decode(NULL,sizeof(input),owned,sizeof(owned),&b));printf("G8_SLOT_SYNTHETIC_PASS checks=%u aliasRejects=3 placeholderRejected=1\n",checks);return 0;}
