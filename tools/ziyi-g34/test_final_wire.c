/* SPDX-License-Identifier: GPL-2.0-only */
#include "final_wire.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void put(uint8_t*p,unsigned n,uint32_t v){for(unsigned j=0;j<4;j++)p[n*4+j]=(uint8_t)(v>>(8*j));}
static void inner(uint8_t*p,unsigned n,uint32_t magic){memset(p,0,n);put(p,0,magic);put(p,1,1);put(p,2,n);put(p,4,1);put(p,3,g34_crc(p+16,n-16));}
int main(void){
 uint8_t a[320],b[80],c[160],d[256],r[G34_FINAL_BYTES],q[128],again[G34_FINAL_BYTES];
 inner(a,320,0x31504347u);inner(b,80,0x314b4347u);inner(c,160,0x31525043u);inner(d,256,0x31525447u);
 assert(g34_final_pack(r,sizeof(r),0x218c2a04bull,0,1,a,b,c,d));
 for(unsigned bit=0;bit<sizeof(r)*8;bit++){r[bit/8]^=(uint8_t)(1u<<(bit%8));assert(!g34_final_valid(r,sizeof(r)));r[bit/8]^=(uint8_t)(1u<<(bit%8));}
 for(unsigned page=0;page<10;page++){
  assert(g34_final_packet(r,sizeof(r),page,q,sizeof(q)));
  unsigned count=page==9?52:92;memcpy(again+page*92,q+32,count);
  for(unsigned bit=0;bit<1024;bit++){q[bit/8]^=(uint8_t)(1u<<(bit%8));assert(!g34_final_packet_valid(q,sizeof(q)));q[bit/8]^=(uint8_t)(1u<<(bit%8));}
 }
 assert(!memcmp(again,r,sizeof(r)));
 memset(again,0xa5,sizeof(again));assert(!g34_final_pack(again,sizeof(again),1,0,0,a,b,c,d));
 for(unsigned i=0;i<sizeof(again);i++)assert(again[i]==0xa5);
 assert(!g34_final_packet(r,sizeof(r),10,q,sizeof(q)));
 assert(!g34_final_packet(r,sizeof(r),0,r,sizeof(q)));
 printf("G34_FINAL_WIRE_PASS synthetic only: 10 pages, 7040 record + 10240 transport bit refusals\n");
 return 0;
}
