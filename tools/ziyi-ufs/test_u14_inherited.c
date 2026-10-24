#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "u14_ui.h"
#include "u12_input.h"
static unsigned checks;
#define CHECK(x) do{assert(x);checks++;}while(0)
static unsigned rng=0x13579bdu;
static unsigned next(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static void putbits(uint8_t *p,unsigned *at,unsigned n,unsigned value){
 for(unsigned i=0;i<n;i++,(*at)++)if((value>>i)&1)p[*at/8]|=(uint8_t)(1u<<(*at%8));
}
static void input_tests(void){
 CHECK(zi_spi_rate(0,1,0,0x41)==4800000);
 CHECK(!zi_spi_rate(0,1,0,0x31));CHECK(!zi_spi_rate(0,0x101,0,0x41));
 CHECK(!zi_spi_rate(1,1,0,0x41));CHECK(!zi_spi_rate(0,1,1,0x41));
 uint32_t m=0,s=0;CHECK(zi_spmi_layout(0x70000000,&m,&s)==0&&m==0x2000&&s==0x20);
 CHECK(zi_spmi_layout(0x50000000,&m,&s)==0&&m==0x900&&s==0x80);
 CHECK(zi_spmi_layout(0x80000000,&m,&s)!=0);CHECK(zi_spmi_layout(0x40000000,&m,&s)!=0);
 CHECK(zi_key_state(0,128,0)==1&&zi_key_state(2,64,0)==1&&zi_key_state(1,0,1)==0);
 struct zi_key key={0};CHECK(!zi_key_update(&key,1,1));CHECK(zi_key_update(&key,1,1)==1);
 for(unsigned i=0;i<12;i++)CHECK(!zi_key_update(&key,0,0));
 CHECK(zi_key_update(&key,0,0)==-1);CHECK(!zi_key_update(&key,1,1));CHECK(zi_key_update(&key,1,1)==1);
 CHECK(!zi_key_update(&key,1,0));CHECK(zi_key_update(&key,1,0)==-1);
 uint8_t cfg[]={24,8,1,6,4,7,4,8,16,9,16,3,0},p[64]={0};unsigned at=0;
 putbits(p,&at,8,2);putbits(p,&at,4,2);putbits(p,&at,4,1);putbits(p,&at,16,1079);putbits(p,&at,16,2399);
 putbits(p,&at,4,7);putbits(p,&at,4,2);putbits(p,&at,16,550);putbits(p,&at,16,1200);
 struct zi_contact out[ZI_CONTACTS];memset(out,0xa5,sizeof(out));
 CHECK(!zi_tcm_report(cfg,sizeof(cfg),p,(at+7)/8,out));
 CHECK(out[2].down&&out[2].x==1079&&out[2].y==2399&&out[7].down&&out[7].x==550);
 for(unsigned i=0;i<ZI_CONTACTS;i++)if(i!=2&&i!=7)CHECK(!out[i].down);
 uint8_t empty[1]={0};CHECK(!zi_tcm_report(cfg,sizeof(cfg),empty,1,out));
 for(unsigned i=0;i<ZI_CONTACTS;i++)CHECK(!out[i].down);
 struct zi_contact before[ZI_CONTACTS];memset(out,0x5a,sizeof(out));memcpy(before,out,sizeof(out));
 for(unsigned n=0;n<(at+7)/8;n++){CHECK(zi_tcm_report(cfg,sizeof(cfg),p,n,out)!=0);CHECK(!memcmp(out,before,sizeof(out)));}
 for(unsigned i=0;i<sizeof(cfg)-1;i++){CHECK(zi_tcm_report(cfg,i,p,sizeof(p),out)!=0);CHECK(!memcmp(out,before,sizeof(out)));}
 for(unsigned i=0;i<5000;i++){
  uint8_t c[64],data[64];for(unsigned j=0;j<64;j++){c[j]=(uint8_t)next();data[j]=(uint8_t)next();}
  int result=zi_tcm_report(c,next()%64,data,next()%64,out);
  if(result)CHECK(!memcmp(out,before,sizeof(out)));
  else {for(unsigned k=0;k<ZI_CONTACTS;k++)CHECK(out[k].x<=1079&&out[k].y<=2399);memcpy(out,before,sizeof(out));}
 }
}
static void ui_tests(void){
 uint8_t src[16*16*4],dst[16*16*4+32],before[sizeof(dst)];
 for(unsigned i=0;i<sizeof(src);i++)src[i]=(uint8_t)next();
 struct zui_image a={src,sizeof(src),0,16,16,64,ZUI_BGRA};
 struct zui_target d={dst+16,16*16*4,16,16,64};struct zui_plan plan;
 for(unsigned i=0;i<3000;i++){
  memset(dst,0x5a,sizeof(dst));memcpy(before,dst,sizeof(dst));
  struct zui_rect sr={0,0,16,16},dr={(int32_t)(next()%49)-24,(int32_t)(next()%49)-24,(int32_t)(next()%32+1),(int32_t)(next()%32+1)};
  int result=zui_prepare(&a,&d,sr,dr,&plan);
  if(!result){
   zui_render(&a,&d,&plan,0);
   for(int y=0;y<16;y++)for(int x=0;x<16;x++){
    uint8_t *q=before+16+y*64+x*4;
    if(x>=dr.x&&y>=dr.y&&(int64_t)x<dr.x+dr.w&&(int64_t)y<dr.y+dr.h){
     unsigned sx=(unsigned)((int64_t)(x-dr.x)*16/dr.w),sy=(unsigned)((int64_t)(y-dr.y)*16/dr.h);
     memcpy(q,src+sy*64+sx*4,4);
    }
   }
  }
  CHECK(!memcmp(dst,before,sizeof(dst)));
 }
 struct zui_rect full={0,0,16,16};memset(dst,0x5a,sizeof(dst));
 a.offset=1;CHECK(zui_prepare(&a,&d,full,full,&plan)!=0);a.offset=0;
 a.format=0;CHECK(zui_prepare(&a,&d,full,full,&plan)!=0);a.format=ZUI_BGRA;
 CHECK(zui_prepare(&a,&d,(struct zui_rect){INT_MAX,0,1,1},full,&plan)!=0);
 CHECK(zui_prepare(&a,&d,full,(struct zui_rect){INT_MAX,INT_MIN,16,16},&plan)!=0);
 uint32_t formats[]={ZUI_BGRA,ZUI_RGBA,ZUI_ARGB,ZUI_ABGR};
 uint8_t pixels[][4]={{30,20,10,128},{10,20,30,128},{128,10,20,30},{128,30,20,10}};
 for(unsigned i=0;i<4;i++){
  a=(struct zui_image){pixels[i],4,0,1,1,4,formats[i]};d=(struct zui_target){dst+16,4,1,1,4};full=(struct zui_rect){0,0,1,1};
  CHECK(!zui_prepare(&a,&d,full,full,&plan));zui_render(&a,&d,&plan,0);
  CHECK(dst[16]==30&&dst[17]==20&&dst[18]==10&&dst[19]==128);
  memset(dst+16,100,4);zui_render(&a,&d,&plan,1);
  CHECK(dst[16]==80&&dst[17]==70&&dst[18]==60&&dst[19]==255);
 }
}
int main(void){input_tests();ui_tests();printf("U14_INHERITED_UI_INPUT_PASS checks=%u\n",checks);return 0;}
