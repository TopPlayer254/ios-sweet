#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "u12_input.h"
#include "u23_native_touch.h"
static unsigned checks;
#define CHECK(x) do{assert(x);checks++;}while(0)
static void bits(uint8_t *p,unsigned *at,unsigned width,unsigned v) {
 for(unsigned i=0;i<width;i++,(*at)++)p[*at/8]|=(uint8_t)(((v>>i)&1)<<(*at%8));
}
static unsigned contact(uint8_t *p,unsigned count,unsigned id,unsigned state,unsigned x,unsigned y) {
 unsigned at=0;memset(p,0,64);bits(p,&at,8,count);
 if(count){bits(p,&at,4,id);bits(p,&at,4,state);bits(p,&at,16,x);bits(p,&at,16,y);}
 return (at+7)/8;
}
int main(void) {
 uint8_t cfg[]={24,8,1,6,4,7,4,8,16,9,16,3,0},p[64];
 struct zi_contact out[ZI_CONTACTS],before[ZI_CONTACTS];
 CHECK(zi_tcm_geometry(10799,23999));CHECK(zi_tcm_geometry(1079,2399));
 CHECK(!zi_tcm_geometry(10799,2399));CHECK(!zi_tcm_geometry(10800,24000));CHECK(!zi_tcm_geometry(0,0));
 const unsigned raw[][4]={{0,0,0,0},{9,9,0,0},{10,10,1,1},{5399,11999,539,1199},{5400,12000,540,1200},{10790,23990,1079,2399},{10799,23999,1079,2399}};
 for(unsigned i=0;i<sizeof(raw)/sizeof(raw[0]);i++) {
  unsigned n=contact(p,1,0,1,raw[i][0],raw[i][1]);
  CHECK(!zi_tcm_report_native(cfg,sizeof(cfg),p,n,out,10799,23999));
  CHECK(out[0].down&&out[0].x==raw[i][2]&&out[0].y==raw[i][3]);
  for(unsigned j=1;j<ZI_CONTACTS;j++)CHECK(!out[j].down);
 }
 /* A real report sequence for a bottom-to-top gesture, without inventing
  * contacts in the driver. Native last row maps exactly to HID's last row. */
 unsigned previous=2400;
 const unsigned ys[]={23999,22000,18000,14000,10000,6000,2000,0};
 for(unsigned i=0;i<sizeof(ys)/sizeof(ys[0]);i++) {
  unsigned n=contact(p,1,3,1,5400,ys[i]);
  CHECK(!zi_tcm_report_native(cfg,sizeof(cfg),p,n,out,10799,23999));
  CHECK(out[3].down&&out[3].x==540&&out[3].y<previous);previous=out[3].y;
 }
 unsigned n=contact(p,0,0,0,0,0);CHECK(!zi_tcm_report_native(cfg,sizeof(cfg),p,n,out,10799,23999));
 for(unsigned i=0;i<ZI_CONTACTS;i++)CHECK(!out[i].down);
 for(unsigned fault=0;fault<7;fault++) {
  memset(out,0x5a,sizeof(out));memcpy(before,out,sizeof(out));
  n=contact(p,1,0,1,100,200);uint32_t mx=10799,my=23999;
  if(fault==0)n=contact(p,1,0,1,10800,200);
  if(fault==1)n=contact(p,1,0,1,100,24000);
  if(fault==2)n--;
  if(fault==3)n=contact(p,1,10,1,100,200);
  if(fault==4)mx=65535;
  if(fault==5)my=0;
  if(fault==6)n=contact(p,11,0,1,100,200);
  CHECK(zi_tcm_report_native(cfg,sizeof(cfg),p,n,out,mx,my));CHECK(!memcmp(out,before,sizeof(out)));
 }
 /* Legacy geometry remains byte-for-byte equal to the inherited parser for
  * valid and malformed payloads; this includes all its error semantics. */
 uint32_t rng=1234567;
 for(unsigned i=0;i<4000;i++) {
  for(unsigned j=0;j<64;j++){rng=rng*1664525+1013904223;p[j]=(uint8_t)(rng>>24);}
  if(i%2==0)contact(p,1,i%10,1,i%1080,i%2400);
  unsigned len=i%2==0?6:i%64;
  struct zi_contact old[ZI_CONTACTS];memset(old,0x5a,sizeof(old));memset(out,0x5a,sizeof(out));
  int a=zi_tcm_report(cfg,sizeof(cfg),p,len,old),b=zi_tcm_report_native(cfg,sizeof(cfg),p,len,out,1079,2399);
  CHECK(a==b);CHECK(!memcmp(old,out,sizeof(old)));
 }
 printf("U23_NATIVE_TCM_GEOMETRY_PASS checks=%u measured10799x23999 edges/gesture/release transaction/legacy rejects\n",checks);
 return 0;
}
