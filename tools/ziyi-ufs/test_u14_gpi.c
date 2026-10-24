#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "u14_gpi.h"
static uint32_t regs[0x18000];static unsigned phase,mutate,reads,waits;
static uint32_t rd(void *ctx,uint32_t o) {
 (void)ctx;assert(o<0x60000&&o>=0x10000&&!(o&3));reads++;
 if(phase&&mutate&&o==0x10010)return regs[o/4]^16;
 return regs[o/4];
}
static void wait_us(void *ctx,unsigned n){(void)ctx;assert(n==50);phase=1;waits++;}
static void reset(void){memset(regs,0,sizeof(regs));phase=mutate=reads=waits=0;}
int main(void) {
 struct zg_io io={NULL,rd,wait_us};struct zg_snapshot s;unsigned checks=0;
 reset();assert(!zg_quiet(&io,&s)&&reads==196&&waits==1);checks++;
 for(unsigned n=0;n<7;n++)for(unsigned ch=0;ch<2;ch++)for(unsigned state=0;state<16;state++) {
  reset();regs[(0x10000+n*0x4000+ch*0x80)/4]=0x10000002|(state<<20);
  int e=zg_quiet(&io,&s);assert((e==0)==(state==0||state==1||state==3));checks++;
 }
 for(unsigned n=0;n<7;n++)for(unsigned ch=0;ch<2;ch++) {
  reset();regs[(0x10000+n*0x4000+ch*0x80)/4]=0x08000002|(1u<<20);
  assert(zg_quiet(&io,&s)&4);checks++;
  reset();regs[(0x10000+n*0x4000+ch*0x80)/4]=0x10000001|(3u<<20);
  assert(zg_quiet(&io,&s)&4);checks++;
 }
 reset();mutate=1;assert(zg_quiet(&io,&s)&8);checks++;
 assert(zg_quiet(NULL,&s)==1);checks++;
 printf("U14_READONLY_GPI_POLICY_PASS checks=%u repeatedWords=98 writes=0\n",checks);
 return 0;
}
