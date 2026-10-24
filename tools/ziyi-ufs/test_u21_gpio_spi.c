#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "u21_gpio_spi.h"
struct model {
 uint32_t se[0x1000/4],gpio[128][2],before[128][2];
 unsigned writes,fail_at,quiet,run,edges,cs_low,cs_high,waits,reset_low;
 uint8_t input[4099],output[4099];
};
static struct model m;
static unsigned checks;
#define CHECK(x) do {assert(x);checks++;} while(0)
static uint32_t rd(void *ctx,unsigned b,uint32_t o) {
 struct model *p=ctx;
 if(b==BB_SE){assert(o<0x1000&&!(o&3));return p->se[o/4];}
 assert(b==BB_GPIO && o/0x1000<128 && (o%0x1000==0||o%0x1000==4));
 if(o==4&&p->run&&p->edges) {
  assert(!(p->gpio[3][1]&2));assert(p->gpio[2][1]&2);
  unsigned bit=p->edges-1;
  return (p->gpio[0][1]&~1u)|((p->input[bit/8]>>(7-bit%8))&1);
 }
 return p->gpio[o/0x1000][o%0x1000/4];
}
static void wr(void *ctx,unsigned b,uint32_t o,uint32_t v) {
 struct model *p=ctx;unsigned n=++p->writes;
 if(b==BB_SE) {
  assert(o==0xe18||o==0x258);p->se[o/4]=v;return;
 }
 unsigned pin=o/0x1000,reg=o%0x1000;
 assert(b==BB_GPIO&&pin<128&&(pin<4||pin==126)&&(reg==0||reg==4));
 assert(!(pin==0&&reg==4)); /* MISO output latch is never touched. */
 if(pin==0)assert(!(v&0x200));
 if(pin==126&&reg==4&&!(v&2))p->reset_low++;
 uint32_t old=p->gpio[pin][reg/4];
 if(n==p->fail_at)v^=(reg==4?2u:4u);
 p->gpio[pin][reg/4]=v;
 if(p->run&&reg==4) {
  if(pin==3&&((old^v)&2)) {if(v&2)p->cs_high++;else p->cs_low++;}
  if(pin==2&&!(old&2)&&(v&2)) {
   assert(!(p->gpio[3][1]&2));unsigned bit=p->edges++;
   assert(bit<sizeof(p->output)*8);
   p->output[bit/8]=(uint8_t)((p->output[bit/8]<<1)|((p->gpio[1][1]>>1)&1));
  }
 }
}
static void wait_us(void *ctx,unsigned us){struct model *p=ctx;assert(us==2||us==200000);p->waits++;}
static int quiet(void *ctx){return ((struct model *)ctx)->quiet;}
static struct bb_io io={&m,rd,wr,wait_us,quiet};
static void reset(void) {
 memset(&m,0,sizeof(m));m.quiet=1;m.se[0xe18/4]=15;m.se[0x258/4]=1;
 for(unsigned i=0;i<4;i++){m.gpio[i][0]=0x804;m.gpio[i][1]=2;}
 m.gpio[126][0]=0x800;m.gpio[126][1]=2;
 memcpy(m.before,m.gpio,sizeof(m.gpio));
}
static void restored(void) {
 CHECK(!memcmp(m.before,m.gpio,sizeof(m.gpio)));
 CHECK(m.se[0xe18/4]==15&&m.se[0x258/4]==1);
}
int main(void) {
 reset();struct bb_state s={0};CHECK(!bb_take(&io,&s));
 CHECK(!m.reset_low);CHECK(m.gpio[0][0]==0x800);CHECK(m.gpio[1][0]==0xa00);
 CHECK(bb_take(&io,&s)==1);CHECK(!bb_release(&io,&s));restored();
 for(unsigned n=1;n<=4099;n=n==1?3:n==3?4:4099) {
  uint8_t tx[4099],rx[4099];reset();memset(&s,0,sizeof(s));CHECK(!bb_take(&io,&s));
  for(unsigned i=0;i<n;i++){tx[i]=(uint8_t)(i*17+0xa6);m.input[i]=(uint8_t)(i*29+0x5a);}
  m.run=1;CHECK(!bb_transfer(&io,&s,tx,rx,n));m.run=0;
  CHECK(!memcmp(tx,m.output,n));CHECK(!memcmp(rx,m.input,n));
  CHECK(m.edges==n*8&&m.cs_low==1&&m.cs_high==1);
  CHECK((m.gpio[3][1]&2)&&!(m.gpio[2][1]&2));
  CHECK(!bb_release(&io,&s));restored();if(n==4099)break;
 }
 reset();memset(&s,0,sizeof(s));CHECK(!bb_take(&io,&s));m.run=1;
 CHECK(!bb_transfer(&io,&s,NULL,NULL,2));m.run=0;
 CHECK(m.output[0]==255&&m.output[1]==255);CHECK(!bb_release(&io,&s));restored();
 for(unsigned bad=0;bad<9;bad++) {
  reset();memset(&s,0,sizeof(s));
  if(bad==0)m.se[0x40/4]=1;if(bad==1)m.se[0x40/4]=0x1000;
  if(bad==2)m.se[0x258/4]=2;if(bad==3)m.se[0xe18/4]=16;
  if(bad==4)m.quiet=0;if(bad==5)m.gpio[0][0]=8;
  if(bad==6)m.gpio[3][0]=8;if(bad==7)m.gpio[126][0]=4;
  if(bad==8)m.gpio[51][0]=0x200;
  CHECK(bb_take(&io,&s));CHECK(!m.writes);CHECK(!s.saved);
 }
 /* Partial acquisition: every preload/mux/reset write is restored. */
 reset();memset(&s,0,sizeof(s));CHECK(!bb_take(&io,&s));unsigned taken=m.writes;
 for(unsigned f=3;f<=taken;f++) {
  reset();memset(&s,0,sizeof(s));m.fail_at=f;CHECK(bb_take(&io,&s));
  m.fail_at=0;CHECK(!bb_release(&io,&s));restored();
 }
 /* A failed clock/data latch poisons the session: no retry transfer. */
 for(unsigned f=1;f<=6;f++) {
  reset();memset(&s,0,sizeof(s));CHECK(!bb_take(&io,&s));m.fail_at=m.writes+f;
  uint8_t tx=0xa6;CHECK(bb_transfer(&io,&s,&tx,NULL,1));CHECK(s.poison);
  unsigned writes=m.writes;CHECK(bb_transfer(&io,&s,&tx,NULL,1));CHECK(m.writes==writes);
  CHECK(bb_release(&io,&s));CHECK(m.writes==writes);
 }
 reset();memset(&s,0,sizeof(s));CHECK(!bb_take(&io,&s));unsigned writes=m.writes;
 CHECK(bb_transfer(&io,&s,NULL,NULL,0));CHECK(bb_transfer(&io,&s,NULL,NULL,4100));
 CHECK(m.writes==writes);s.poison=1;CHECK(bb_release(&io,&s));CHECK(m.writes==writes);
 printf("U21_GPIO_RESTORE_PASS checks=%u actual_header mode0 MSB duplex idle/bounds/partial-restore/poison no-MISO-drive\n",checks);
 return 0;
}
