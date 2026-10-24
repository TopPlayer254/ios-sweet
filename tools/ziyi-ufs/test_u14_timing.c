#include <assert.h>
#include <stdio.h>
#include "u14_timing.h"
int main(void) {
 unsigned checks=0;
 const struct {uint64_t previous,now,period,want;} cases[]={
  {100,100,10,110},{100,103,10,110},{100,109,10,110},
  {100,110,10,120},{100,119,10,120},{100,135,10,140},
  {100,199,10,200},{100,200,10,210},{100,99,10,110},
  {0,1000,10,1010},{100,100,0,0},{UINT64_MAX,10,1,0},
  {0,UINT64_MAX,1,0},{UINT64_MAX-1,UINT64_MAX-1,1,UINT64_MAX}};
 for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
  assert(zv_next_deadline(cases[i].previous,cases[i].now,cases[i].period)==cases[i].want);checks++;
 }
 /* 60Hz phase stays fixed when callbacks do work, unlike now+period. */
 uint64_t phase=1000000;
 for(unsigned n=0;n<10000;n++) {
  uint64_t work=n%4==0?40000:n%4==1?10000:n%4==2?16667:1;
  uint64_t end=phase+work,next=zv_next_deadline(phase,end,16667);
  assert(next>end&&next-end<=16667&&(next-1000000)%16667==0);checks++;
  phase=next;
 }
 printf("U14_ABSOLUTE_VSYNC_POLICY_PASS checks=%u noCatchupStorm=PASS\n",checks);
 return 0;
}
