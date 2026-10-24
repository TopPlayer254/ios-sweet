#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "u21_power.h"
struct model {uint32_t db[0x20000/4],rsc[0x10000/4];unsigned writes,issued,busy,timeout_step,bad_readback;};
static struct model m;static unsigned checks;
#define CHECK(x) do{assert(x);checks++;}while(0)
static uint32_t read_word(void *ctx,unsigned b,uint32_t o) {
 struct model *p=ctx;assert(!(o&3));
 if(b==ZP_DB){assert(o<0x20000);return p->db[o/4];}
 assert(b==ZP_RSC&&o<0x10000);
 if(o==0xd04&&p->busy&&p->issued!=p->timeout_step) {
  if(!--p->busy){p->rsc[0xd04/4]=2;p->rsc[(ZP_SLOT+0x18)/4]=1;p->rsc[(ZP_SLOT+0x3c)/4]=0x10100;}
 }
 if(p->bad_readback&&o==ZP_SLOT+0x34&&p->rsc[o/4])return p->rsc[o/4]^4;
 return p->rsc[o/4];
}
static void write_word(void *ctx,unsigned b,uint32_t o,uint32_t v) {
 struct model *p=ctx;CHECK(b==ZP_RSC);p->writes++;
 if(o==0xd08){CHECK(v==2);p->rsc[0xd04/4]&=~v;return;}
 CHECK(o==ZP_SLOT+0x14||o==ZP_SLOT+0x1c||o==ZP_SLOT+0x30||o==ZP_SLOT+0x34||o==ZP_SLOT+0x38);
 if(o==ZP_SLOT+0x14)CHECK(v==0||v==0x10000||v==0x1010000);
 if(o==ZP_SLOT+0x1c)CHECK(v<=1);
 if(o==ZP_SLOT+0x30)CHECK(v==0||v==0x10108);
 if(o==ZP_SLOT+0x34)CHECK(v==0||v==0x42100||v==0x42104||v==0x42108||v==0x41300||v==0x41304||v==0x41308);
 p->rsc[o/4]=v;
 if(o==ZP_SLOT+0x14&&v==0x1010000) {
  static const uint32_t addr[]={0x42100,0x42108,0x42104,0x41300,0x41308,0x41304},value[]={1800,7,1,2800,7,1};
  CHECK(p->issued<6&&p->rsc[(ZP_SLOT+0x34)/4]==addr[p->issued]&&p->rsc[(ZP_SLOT+0x38)/4]==value[p->issued]);
  CHECK(p->rsc[(ZP_SLOT+0x30)/4]==0x10108&&p->rsc[(ZP_SLOT+0x1c)/4]==1&&p->rsc[0xd04/4]==0);
  p->issued++;p->busy=3;p->rsc[(ZP_SLOT+0x18)/4]=0;p->rsc[(ZP_SLOT+0x3c)/4]=0;
 }
}
static void wait_us(void *ctx,unsigned us){(void)ctx;assert(us==1||us==50||us==20000||us==200000);}
static struct zp_io io={&m,read_word,write_word,wait_us};
static void reset(void) {
 memset(&m,0,sizeof(m));m.timeout_step=~0u;m.db[0]=1;m.db[1]=0x0c0330db;
 m.db[14]=0x08380004;m.db[15]=0x00410e50;m.db[16]=0x100;
 for(unsigned i=0;i<2;i++) {
  unsigned at=(144+0x838+i*24)/4;m.db[at]=0x636f646c;m.db[at+1]=i?0x3131:0x32;
  m.db[at+4]=i?0x41300:0x42100;m.db[at+5]=(i?0xec0000u:0xc80000u)|4;
  m.db[(144+0xe50+(i?0xec:0xc8))/4]=i?0x3001:0x2701;
 }
 m.rsc[4/4]=0x04010100;m.rsc[0xc/4]=0x800c8104;m.rsc[0xd00/4]=1;
 for(unsigned i=0;i<8;i++)m.rsc[(0xd00+i*0x2a0+0x18)/4]=1;
 m.rsc[(0xd00+0x34)/4]=0x50000;
}
int main(void) {
 struct zp_state s={0};reset();CHECK(!zp_start(&io,&s));CHECK(s.steps==6&&!s.poison&&m.issued==6);
 CHECK(!zp_idle(&io));CHECK(m.rsc[0xd00/4]==1&&m.rsc[0xd04/4]==0);
 for(unsigned i=0;i<16;i++)for(unsigned k=0;k<3;k++)CHECK(m.rsc[(ZP_SLOT+0x30+i*0x14+k*4)/4]==s.saved[i][k]);
 unsigned writes=m.writes;CHECK(zp_vote(&io,&s,6));CHECK(m.writes==writes);CHECK(zp_start(&io,&s));CHECK(m.writes==writes);
 for(unsigned bad=0;bad<10;bad++) {
  reset();memset(&s,0,sizeof(s));
  if(bad==0)m.db[1]^=1;if(bad==1)m.db[(144+0x838)/4+4]^=0x100;
  if(bad==2)m.db[(144+0xe50+0xc8)/4]^=1;
  if(bad==3)m.db[16]=0x200;if(bad==4)m.rsc[0xd04/4]=1;
  if(bad==5)m.rsc[0xd00/4]=3;if(bad==6)m.rsc[(0xd00+3*0x2a0+0x14)/4]=0x10000;
  if(bad==7)m.rsc[(ZP_SLOT+0x30+15*0x14)/4]=1;
  if(bad==8)m.rsc[0xc/4]^=0x1000;
  if(bad==9)m.db[(144+0x838+24)/4+1]=0x32;
  CHECK(zp_start(&io,&s));CHECK(m.writes==0&&m.issued==0);
 }
 for(unsigned failure=1;failure<=6;failure++) {
  reset();memset(&s,0,sizeof(s));m.timeout_step=failure;CHECK(zp_start(&io,&s));
  CHECK(s.poison&&s.steps==failure-1&&m.issued==failure);
  writes=m.writes;CHECK(zp_vote(&io,&s,s.steps));CHECK(m.writes==writes);
 }
 reset();memset(&s,0,sizeof(s));m.bad_readback=1;CHECK(zp_start(&io,&s));CHECK(s.poison&&m.issued==0&&s.steps==0);
 reset();memset(&s,0,sizeof(s));CHECK(zp_vote(&io,&s,0));CHECK(m.writes==0);
 printf("U21_FIXED_RPMH_POWER_PASS checks=%u exact-resource/voltage/order slot-backup/restore prewrite-rejects timeout-poison no-IRQ-mask/DMA/media\n",checks);
 return 0;
}
