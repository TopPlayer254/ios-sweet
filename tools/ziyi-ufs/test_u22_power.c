#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "u22_power.h"
struct model {
 uint32_t db[0x20000/4],rsc[0x10000/4];
 unsigned writes,issued,busy,timeout_step,bad_readback,unmasked,stale,partial,not_idle,instant;
};
static struct model m;static unsigned checks;
#define CHECK(x) do{assert(x);checks++;}while(0)
static void complete(struct model *p) {
 p->rsc[0xd04/4]=p->unmasked?2:0;
 p->rsc[(ZP_SLOT+0x18)/4]=p->not_idle?0:1;
 p->rsc[(ZP_SLOT+0x3c)/4]=p->partial?p->partial:0x10100;
 p->rsc[(ZP_SLOT+0x40)/4]=0xface0000+p->issued;
}
static uint32_t read_word(void *ctx,unsigned b,uint32_t o) {
 struct model *p=ctx;assert(!(o&3));
 if(b==ZP_DB){assert(o<0x20000);return p->db[o/4];}
 assert(b==ZP_RSC&&o<0x10000);
 if(p->bad_readback&&o==ZP_SLOT+0x34&&p->rsc[o/4])return p->rsc[o/4]^4;
 return p->rsc[o/4];
}
static void write_word(void *ctx,unsigned b,uint32_t o,uint32_t v) {
 struct model *p=ctx;CHECK(b==ZP_RSC);p->writes++;
 if(o==0xd08){CHECK(v==2&&p->rsc[0xd04/4]==2);p->rsc[0xd04/4]&=~v;return;}
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
  p->issued++;
  if(p->stale)return; /* Old COMPLETE/IDLE without a fresh hardware request. */
  p->busy=p->instant?0:3;
  p->rsc[(ZP_SLOT+0x18)/4]=0;p->rsc[(ZP_SLOT+0x3c)/4]=0;
  if(p->instant)complete(p);
 }
}
static void wait_us(void *ctx,unsigned us) {
 struct model *p=ctx;assert(us==1||us==50||us==20000||us==200000);
 if(us==1&&p->busy&&p->issued!=p->timeout_step&&!--p->busy)complete(p);
}
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
 struct zp_state s;unsigned writes;
 for(unsigned unmasked=0;unmasked<2;unmasked++) {
  reset();memset(&s,0,sizeof(s));m.unmasked=unmasked;
  CHECK(!zp_start(&io,&s));CHECK(s.steps==6&&!s.poison&&m.issued==6);
  CHECK(!zp_idle(&io));CHECK(m.rsc[0xd00/4]==1&&m.rsc[0xd04/4]==0);
  for(unsigned i=0;i<6;i++)CHECK(s.status[i]==0x10100&&s.response[i]==0xface0001+i);
  for(unsigned i=0;i<16;i++)for(unsigned k=0;k<3;k++)CHECK(m.rsc[(ZP_SLOT+0x30+i*0x14+k*4)/4]==s.saved[i][k]);
  writes=m.writes;CHECK(zp_vote(&io,&s,6));CHECK(m.writes==writes);CHECK(zp_start(&io,&s));CHECK(m.writes==writes);
 }
 for(unsigned bad=0;bad<10;bad++) {
  reset();memset(&s,0,sizeof(s));
  if(bad==0)m.db[1]^=1;if(bad==1)m.db[(144+0x838)/4+4]^=0x100;
  if(bad==2)m.db[(144+0xe50+0xc8)/4]^=1;
  if(bad==3)m.db[16]=0x200;if(bad==4)m.rsc[0xd04/4]=1;
  if(bad==5)m.rsc[0xd00/4]=3;if(bad==6)m.rsc[(0xd00+3*0x2a0+0x14)/4]=0x10000;
  if(bad==7)m.rsc[(ZP_SLOT+0x30+15*0x14)/4]=1;
  if(bad==8)m.rsc[0xc/4]^=0x1000;if(bad==9)m.db[(144+0x838+24)/4+1]=0x32;
  CHECK(zp_start(&io,&s));CHECK(m.writes==0&&m.issued==0);
 }
 for(unsigned failure=1;failure<=6;failure++) {
  reset();memset(&s,0,sizeof(s));m.timeout_step=failure;CHECK(zp_start(&io,&s));
  CHECK(s.poison&&s.steps==failure-1&&m.issued==failure);
  CHECK(s.phase==4&&s.error==6&&s.polls==200000&&s.command==0&&s.tcs_status==0&&s.enable==1);
  CHECK(s.address_word==m.rsc[(ZP_SLOT+0x34)/4]&&s.data_word==m.rsc[(ZP_SLOT+0x38)/4]);
  writes=m.writes;CHECK(zp_vote(&io,&s,s.steps));CHECK(m.writes==writes);
 }
 reset();memset(&s,0,sizeof(s));m.bad_readback=1;CHECK(zp_start(&io,&s));CHECK(s.poison&&m.issued==0&&s.error==3&&s.phase==2);
 reset();memset(&s,0,sizeof(s));m.stale=1;m.rsc[(ZP_SLOT+0x3c)/4]=0x10100;
 CHECK(zp_start(&io,&s));CHECK(s.steps==0&&s.poison&&s.error==6&&!s.fresh&&s.command==0x10100&&s.tcs_status==1);
 for(unsigned fault=0;fault<3;fault++) {
  reset();memset(&s,0,sizeof(s));if(fault<2)m.partial=fault?0x10000:0x100;else m.not_idle=1;
  CHECK(zp_start(&io,&s));CHECK(s.poison&&s.steps==0&&s.error==6);
 }
 reset();memset(&s,0,sizeof(s));m.instant=1;CHECK(!zp_qualify(&io,&s));CHECK(!zp_vote(&io,&s,0));
 CHECK(s.steps==1&&s.fresh);CHECK(zp_vote(&io,&s,1));CHECK(s.steps==1&&s.error==6&&!s.fresh);
 reset();memset(&s,0,sizeof(s));CHECK(zp_vote(&io,&s,0));CHECK(m.writes==0);
 puts("U22_NATIVE_COMPLETION_CASES_PASS masked-IRQ/unmasked stale/partial/busy/timeout no-blind-retry");
 printf("U22_FIXED_RPMH_POWER_PASS checks=%u exact-resource/voltage/order fresh-issued-complete/idle terminal-evidence no-IRQ-mask/DMA/media\n",checks);
 return 0;
}
