#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "u16_board.h"
static uint32_t se[0x1000],gcc[0x19000],gpio[0x40000];
static unsigned writes,delay_count,fault,checks,root_update,gpi_ok;
static uint32_t rd(void *ctx,unsigned b,uint32_t o) {
 (void)ctx;assert(!(o&3));
 if(b==ZB_SE){assert(o<sizeof(se));return se[o/4];}
 if(b==ZB_GPIO){assert(o<sizeof(gpio));return gpio[o/4];}
 assert(b==ZB_GCC&&o<sizeof(gcc));
 if(o==0x27014&&(gcc[o/4]&1)&&root_update&&fault!=2){gcc[o/4]&=~0x80000001u;root_update=0;}
 if((o==0x27004||o==0x27008||o==0x33000||o==0x3300c||o==0x2700c)&&fault==1)return 0x80000000;
 return gcc[o/4];
}
static void wr(void *ctx,unsigned b,uint32_t o,uint32_t v) {
 (void)ctx;writes++;assert(!(o&3));
 if(b==ZB_GCC){assert(o==0x62008||o==0x27018||o==0x27014);gcc[o/4]=v;if(o==0x27014&&(v&1))root_update=1;return;}
 if(b==ZB_SE){assert(o==0x48||o==0x7c||o==0x258||o==0xe18);if(fault!=3)se[o/4]=v;return;}
 assert(b==ZB_GPIO&&(o<=0x3000||o==126*0x1000||o==126*0x1000+4));
 if(fault!=4)gpio[o/4]=v;
}
static void wait_us(void *ctx,unsigned us){(void)ctx;delay_count+=us;}
static int quiet(void *ctx){(void)ctx;return gpi_ok;}
static struct zb_io io={NULL,rd,wr,wait_us,quiet};
static void reset(unsigned mode) {
 memset(se,0,sizeof(se));memset(gcc,0,sizeof(gcc));memset(gpio,0,sizeof(gpio));
 writes=delay_count=root_update=gpi_ok=0;fault=mode;
 se[0x68/4]=0x102;se[0x258/4]=1;se[0x48/4]=1;
 gcc[0x27014/4]=0x80000000;gcc[0x27018/4]=0x100000;gcc[0x62008/4]=0xa0000040;
 for(unsigned i=0;i<4;i++)gpio[i*0x1000/4]=1;
 gpio[126*0x1000/4]=1;gpio[51*0x1000/4]=1;
}
#define CHECK(x) do{assert(x);checks++;}while(0)
int main(void) {
 /* Measured cold U12 handoff -> active TCXO/FIFO pins -> exact restoration. */
 reset(0);struct zb_state s={0};CHECK(!zb_start(&io,&s));
 CHECK((gcc[0x62008/4]&ZB_VOTES)==ZB_VOTES);CHECK(gcc[0x27018/4]==0x1);
 CHECK(!(gcc[0x27014/4]&0x80000001));CHECK(se[0x48/4]==0x41);
 CHECK(zb_spi_rate(gcc[0x27014/4],gcc[0x27018/4],se[0x7c/4],se[0x48/4])==4800000);
 for(unsigned i=0;i<8;i++)if(i!=2)CHECK(!zb_spi_rate(0,0x1,i,0x41));
 CHECK(!zb_spi_rate(0x80000000,0x1,2,0x41));
 CHECK(!zb_spi_rate(0,0x100101,2,0x41));
 CHECK(!zb_spi_rate(0,0x100000,2,0x41));
 CHECK(!zb_spi_rate(0,0x1,2,0x31));
 se[0x258/4]=1;CHECK(zb_connect(&io,&s)==25); /* DMA mode must be switched by serial owner. */
 se[0x258/4]=0;CHECK(!zb_connect(&io,&s));
 for(unsigned i=0;i<4;i++)CHECK(gpio[i*0x1000/4]==0x84);
 CHECK(gpio[126*0x1000/4]==0x200);CHECK(gpio[(126*0x1000+4)/4]&2);
 unsigned before=writes;CHECK(zb_restore(&io,&s,0)==30&&writes==before);
 se[0x40/4]=1;CHECK(zb_restore(&io,&s,1)==30&&writes==before);se[0x40/4]=0;
 gcc[0x62008/4]|=0x8000;CHECK(!zb_restore(&io,&s,1));
 CHECK(gcc[0x62008/4]==0xa0008040);CHECK(gcc[0x27018/4]==0x100000);
 CHECK(!(gcc[0x27014/4]&2));CHECK(se[0x48/4]==1);
 CHECK(gpio[126*0x1000/4]==1&&gpio[(126*0x1000+4)/4]==0);
 for(unsigned i=0;i<4;i++)CHECK(gpio[i*0x1000/4]==1);
 CHECK(!s.saved&&!s.pins&&!s.clocks);
 CHECK(!zb_spi_rate(0,0x100001,2,0x41)); /* Old HW_CLK_CTRL configuration is rejected. */
 /* Every qualification reject is checked before any write. */
 const struct {unsigned b,o;uint32_t value;} bad[]={
  {ZB_SE,0x68,0x302},{ZB_SE,0x40,1},{ZB_SE,0x40,0x1000},
  {ZB_SE,0xe18,1},{ZB_SE,0x800,1},{ZB_SE,0x804,1},
  {ZB_SE,0x258,2},{ZB_GCC,0x27014,1},
  {ZB_GPIO,0,8},{ZB_GPIO,0x1000,12},{ZB_GPIO,0x2000,60},
  {ZB_GPIO,0x3000,16},{ZB_GPIO,126*0x1000,4},
  {ZB_GPIO,51*0x1000,0x200},{ZB_GPIO,51*0x1000,4}};
 for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++) {
  reset(0);s=(struct zb_state){0};uint32_t *p=bad[i].b==ZB_SE?se:bad[i].b==ZB_GCC?gcc:gpio;
  p[bad[i].o/4]=bad[i].value;CHECK(zb_start(&io,&s)!=0&&!writes&&!s.saved);
 }
 /* Timeout/readback failures remain bounded and do not connect the pins. */
 for(unsigned mode=1;mode<=3;mode++) {
  reset(mode);s=(struct zb_state){0};CHECK(zb_start(&io,&s)!=0);
  CHECK(!s.pins&&!s.reset&&delay_count<=40000);
  fault=0;CHECK(!zb_restore(&io,&s,1));CHECK(gcc[0x62008/4]==0xa0000040);
 }
 reset(0);s=(struct zb_state){0};CHECK(!zb_start(&io,&s));se[0x258/4]=0;
 fault=4;CHECK(zb_connect(&io,&s)==26&&!s.reset);fault=0;
 CHECK(!zb_restore(&io,&s,1));
 /* An already released reset never receives a low pulse from connect. */
 reset(0);gpio[(126*0x1000+4)/4]=2;s=(struct zb_state){0};
 CHECK(!zb_start(&io,&s));se[0x258/4]=0;CHECK(!zb_connect(&io,&s));
 CHECK(gpio[(126*0x1000+4)/4]==2);CHECK(!zb_restore(&io,&s,1));
 CHECK(gpio[(126*0x1000+4)/4]==2);
 reset(0);se[0xe18/4]=15;s=(struct zb_state){0};
 CHECK(zb_start(&io,&s)==3&&!writes);
 gpi_ok=1;CHECK(!zb_start(&io,&s));CHECK(!se[0xe18/4]&&!se[0x258/4]);
 se[0x258/4]=0;CHECK(!zb_connect(&io,&s));CHECK(!zb_restore(&io,&s,1));
 CHECK(se[0xe18/4]==15&&se[0x258/4]==1);
 printf("U16_ACTUAL_BOARD_SEQUENCE_PASS checks=%u ownershipRejects=%zu noDMA=PASS boundedRollback=PASS\n",checks,sizeof(bad)/sizeof(bad[0]));
 return 0;
}
