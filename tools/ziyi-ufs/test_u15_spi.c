
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
static unsigned zt_dead,zt_depth=8,mode,active,irq,txlen,rxlen,sent,received,queued,head,cs,writes,errors;
static uint32_t fifo[64];
static uintptr_t zt_se;
static unsigned diag_reads,diag_messages;
static int test_printf(const char *fmt,...) {(void)fmt;diag_messages++;return 0;}
#define printf test_printf
static void delay(unsigned n){(void)n;}
static uint32_t zt_rd(uintptr_t base,unsigned off){
 (void)base;
 if(off==0x40)return active;
 if(off==0x610)return mode==1&&active?8:irq;
 if(off==0x800)return 0;
 if(off==0x804){unsigned n=(queued+3)/4;return n|(queued%4?(0x80000000u|((queued%4)<<28)):0);}
 if(off==0x780){assert(queued);uint32_t v=fifo[head++%64];unsigned n=queued<4?queued:4;queued-=n;received+=n;return v;}
 diag_reads++;return 0;
}
static void zt_wr(unsigned off,uint32_t value){
 writes++;
 assert(off==0x604||off==0x618||off==0x600||off==0x26c||off==0x270||off==0x700);
 if(off==0x26c){txlen=value;return;}if(off==0x270){rxlen=value;return;}
 if(off==0x604){if(mode!=2){active=0;irq=16;}return;}
 if(off==0x618){irq&=~value;return;}
 if(off==0x600){unsigned op=value>>27;
  if(op==8||op==9){cs=op==8;irq=1;active=0;return;}
  assert(cs&&(op==1||op==3));sent=received=queued=head=0;active=1;irq=0;return;
 }
 if(off==0x700){
  assert(active);unsigned n=txlen-sent;if(n>4)n=4;
  if(!n){errors++;return;}
  if(rxlen){uint32_t word=0;for(unsigned k=0;k<n;k++)word|=(uint32_t)((sent+k)*13u&255u)<<(k*8);fifo[(head+(queued+3)/4)%64]=word;queued+=n;assert(queued<=zt_depth*4);}
  sent+=n;if(sent==txlen){active=0;irq=1;}
 }
}
/* Snapshot status/configuration only: never read RX FIFO or issue a command.
 * Keep the pre-cancel state rather than diagnosing the already-aborted engine.
 * At most four failure records, then stop logging even if a report repeats. */
static void zt_fault(unsigned stage,unsigned detail,unsigned sent,unsigned received) {
 static unsigned records;
 if(records++>=4)return;
 printf("ZIYI U15 SPI fault stage=%u detail=%u sent=%u received=%u dead=%u\n",stage,detail,sent,received,zt_dead);
 printf("ZIYI U15 SPI state=%x cmd=%x ctl=%x irq=%x enable=%x route=%x gsi=%x\n",zt_rd(zt_se,0x40),zt_rd(zt_se,0x600),zt_rd(zt_se,0x604),zt_rd(zt_se,0x610),zt_rd(zt_se,0x614),zt_rd(zt_se,0xe1c),zt_rd(zt_se,0xe18));
 printf("ZIYI U15 SPI tx=%x rx=%x txlen=%x rxlen=%x ios=%x mode=%x\n",zt_rd(zt_se,0x800),zt_rd(zt_se,0x804),zt_rd(zt_se,0x26c),zt_rd(zt_se,0x270),zt_rd(zt_se,0x908),zt_rd(zt_se,0x258));
 printf("ZIYI U15 SPI clock=%x select=%x word=%x pack=%x/%x rxpack=%x/%x\n",zt_rd(zt_se,0x48),zt_rd(zt_se,0x7c),zt_rd(zt_se,0x268),zt_rd(zt_se,0x260),zt_rd(zt_se,0x264),zt_rd(zt_se,0x284),zt_rd(zt_se,0x288));
}
static int zt_abort(void) {
 zt_wr(0x604,4);
 for(unsigned i=0;i<1000;i++){if(!(zt_rd(zt_se,0x40)&1))return 0;delay(1);}
 zt_wr(0x604,2);
 for(unsigned i=0;i<1000;i++){if(!(zt_rd(zt_se,0x40)&1))return 0;delay(1);}
 zt_dead=1;return 1;
}
static int zt_cs(unsigned assert_cs) {
 if(zt_dead||(zt_rd(zt_se,0x40)&1)){zt_fault(assert_cs?1:2,1,0,0);return 1;}
 zt_wr(0x618,0x7fffffff);zt_wr(0x600,(assert_cs?8u:9u)<<27);
 for(unsigned i=0;i<10000;i++) {
  uint32_t irq=zt_rd(zt_se,0x610);
  if(irq&0x3e){zt_fault(assert_cs?1:2,2,0,0);(void)zt_abort();return 2;}
  if(irq&1){zt_wr(0x618,irq);return 0;}
  delay(1);
 }
 zt_fault(assert_cs?1:2,3,0,0);(void)zt_abort();return 3;
}
static int zt_transfer(const uint8_t *tx,uint8_t *rx,size_t bytes) {
 if(!bytes||bytes>4099||zt_dead)return 1;
 if(zt_cs(1))return 2;
 unsigned sent=0,received=0,reason=1;
 zt_wr(0x26c,(uint32_t)bytes);zt_wr(0x270,rx?(uint32_t)bytes:0);
 zt_wr(0x618,0x7fffffff);
 /* Pinned Qualcomm setup_fifo_xfer uses TX_ONLY|RX_ONLY for duplex. */
 zt_wr(0x600,((rx?3u:1u)<<27)|4u);
 for(unsigned poll=0;poll<100000;poll++) {
  uint32_t irq=zt_rd(zt_se,0x610);
  if(irq&0x3300003e){reason=2;goto bad;}
  uint32_t used=zt_rd(zt_se,0x800)&0x0fffffff;
  if(used>zt_depth){reason=3;goto bad;}
  while(sent<bytes&&used++<zt_depth) {
   uint32_t word=0;
   for(unsigned k=0;k<4&&sent<bytes;k++,sent++)word|=(uint32_t)(tx?tx[sent]:255)<<(k*8);
   zt_wr(0x700,word);
  }
  uint32_t status=zt_rd(zt_se,0x804),count=status&0x01ffffff;
  if(count>zt_depth){reason=4;goto bad;}
  unsigned available=count*4,last=(status>>28)&7;
  if((status&0x80000000u)&&last&&last<4&&available>=4)available-=4-last;
  for(unsigned i=0;i<count;i++) {
   uint32_t word=zt_rd(zt_se,0x780);
   for(unsigned k=0;k<4&&i*4+k<available;k++) {
    if(!rx||received>=bytes){reason=5;goto bad;}
    rx[received++]=(uint8_t)(word>>(k*8));
   }
  }
  if(irq&1) {
   zt_wr(0x618,irq);
   if(sent!=bytes||(rx&&received!=bytes)){reason=6;goto bad;}
   return zt_cs(0);
  }
  /* Leave level FIFO indications to their condition; clear completed events. */
  zt_wr(0x618,irq&~((1u<<26)|(1u<<27)|(1u<<30)));delay(1);
 }
bad:
 zt_fault(3,reason,sent,received);
 (void)zt_abort();if(!zt_dead)(void)zt_cs(0);return 3;
}
static void reset(unsigned m){zt_dead=mode=active=irq=txlen=rxlen=sent=received=queued=head=cs=writes=errors=0;mode=m;}
int main(void){unsigned checks=0;uint8_t tx[4131],rx[4131];
 unsigned sizes[]={1,2,3,4,5,31,32,33,127,128,129,511,512,4096,4099};
 /* Read-only diagnostic snapshots must not consume data or mutate hardware. */
 reset(0);unsigned before=writes;zt_fault(99,77,11,13);
 assert(writes==before&&!head&&diag_reads==16&&diag_messages==4);checks++;
 for(unsigned k=0;k<10;k++)zt_fault(99,77,11,13);
 assert(writes==before&&!head&&diag_messages==16&&diag_reads==64);checks++;
 for(unsigned j=0;j<sizeof(sizes)/sizeof(sizes[0]);j++){
  unsigned n=sizes[j];memset(tx,0x5a,sizeof(tx));memset(rx,0xa5,sizeof(rx));reset(0);
  assert(!zt_transfer(tx+16,rx+16,n));assert(!cs&&sent==n&&received==n&&!errors);checks++;
  for(unsigned k=0;k<n;k++)assert(rx[k+16]==(uint8_t)(k*13));
  for(unsigned k=0;k<16;k++)assert(rx[k]==0xa5&&rx[n+16+k]==0xa5);checks++;
  reset(0);assert(!zt_transfer(tx+16,NULL,n));assert(!cs&&sent==n&&!received);checks++;
 }
 reset(0);assert(zt_transfer(tx,rx,0)&&!writes);checks++;
 reset(0);assert(zt_transfer(tx,rx,4100)&&!writes);checks++;
 reset(1);assert(zt_transfer(tx,rx,8)&&!zt_dead);checks++;
 reset(2);/* Force incomplete transport, then neither cancel nor abort completes. */
 active=1;assert(zt_abort()&&zt_dead);unsigned saved=writes;assert(zt_transfer(tx,rx,8)&&saved==writes);checks++;
 fprintf(stdout,"U15_ACTUAL_SPI_AND_READONLY_FAULT_PASS checks=%u guards=PASS maxFaultRecords=4\n",checks);return 0;
}

