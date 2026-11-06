/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_hfi_diagnostic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"check failed at %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static struct g19_hfi_runtime runtime;
static struct g19_hfi_record record;
static struct g19_hfi_config config;
struct model {
 uint32_t mem[GN_HFI_BYTES/4],events,fail,guards,lose,starts,bells,stores,wrong;
 uint32_t reply_words,remote,version,stale,malformed_index,delays,reply_after;
 uint32_t no_reply,pre_ready,never_ready,reset,fw,unknown,expire_after_consume;
 uint32_t tick_calls,rollback_at;uint64_t now,step;
} m;
static int event(void){return ++m.events!=m.fail;}
static int owner(void *ctx,uint64_t epoch){CHECK(ctx==&m);return epoch==7&&++m.guards!=m.lose;}
static void response(void){
 uint32_t at=m.mem[112/4],base=(GN_HFI_QUEUE0+4096)/4;
 uint32_t words=m.reply_words;
 for(unsigned i=0;i<words&&i<19;i++){
  uint32_t value=i==0?(1u|(words<<8)|(1u<<16)):i==1?(record.sent[0]^m.stale):i==2?m.remote:i==3?m.version:0xcafe0000u+i;
  m.mem[base+(at+i)%GN_HFI_WORDS]=value;
 }
 if(!words)m.mem[base+at]=1u|(1u<<16);
 m.mem[116/4]=m.malformed_index?1023:(at+((words+3)&~3u))%GN_HFI_WORDS;
 if(!words)m.mem[116/4]=(at+4)%GN_HFI_WORDS;
 m.mem[64/4]=m.mem[68/4]; /* peer-owned command read index */
}
static int read32(void *ctx,uint32_t at,uint32_t *out){
 CHECK(ctx==&m&&at<GN_HFI_BYTES&&!(at&3));if(!event())return 0;*out=m.mem[at/4];return 1;
}
static int write32(void *ctx,uint32_t at,uint32_t value){
 CHECK(ctx==&m&&at<GN_HFI_BYTES&&!(at&3));if(!event())return 0;
 CHECK(at==68||at==112||(at>=GN_HFI_QUEUE0&&at<GN_HFI_QUEUE0+4096));
 if(at==64||at==116)m.wrong++;m.stores++;m.mem[at/4]=value;return 1;
}
static int order(void *ctx,enum gh_live_order o){CHECK(ctx==&m&&(o==1||o==2));return event();}
static int bell(void *ctx){
 CHECK(ctx==&m);m.bells++;if(!event())return 0;
 CHECK(m.starts==1&&record.q0_write_stores==1&&record.sent[1]==GH_A662_SUPPORTED_VERSION);
 if(!m.no_reply&&!m.reply_after)response();return 1;
}
static int health(void *ctx,struct g19_hfi_health *h){
 CHECK(ctx==&m);*h=(struct g19_hfi_health){m.fw,m.unknown?2u:(m.starts?!m.never_ready:m.pre_ready),m.reset,91,m.unknown?95u:0u};
 return event()&&!m.unknown;
}
static int start(void *ctx){CHECK(ctx==&m);m.starts++;return event();}
static int ticks(void *ctx,uint64_t *out){
 CHECK(ctx==&m);if(!event())return 0;m.tick_calls++;
 if(m.rollback_at==m.tick_calls){*out=m.now>=2?m.now-2:0;return 1;}
 if(m.expire_after_consume&&record.q1_read_stores)m.now=record.start_tick+record.timeout_ticks;
 *out=m.now;m.now+=m.step;return 1;
}
static int delay(void *ctx,uint32_t us){
 CHECK(ctx==&m&&us==config.delay_us);if(!event())return 0;
 m.delays++;if(m.bells&&!m.no_reply&&m.delays==m.reply_after)response();return 1;
}
static struct g19_hfi_ops ops={&m,{&m,owner,read32,write32,order,bell},health,start,ticks,delay};
static void setup(unsigned at){
 memset(&m,0,sizeof(m));memset(&runtime,0,sizeof(runtime));memset(&record,0,sizeof(record));
 config=(struct g19_hfi_config){7,100000,0x60000000,1,100,100};
 m.fw=0x100;m.pre_ready=1;m.reply_words=4;m.version=0x20010009;m.step=1;
 CHECK(gn_hfi_init(m.mem,sizeof(m.mem),config.gmu_address)==0);
 m.mem[64/4]=m.mem[68/4]=at;m.mem[112/4]=m.mem[116/4]=at;
}
static int run(void){return g19_hfi_run(&runtime,&config,&ops,&record);}
static void roundtrip(void){
 uint8_t bytes[G19_HFI_RECORD_BYTES],again[G19_HFI_RECORD_BYTES];struct g19_hfi_record decoded;
 CHECK(g19_hfi_record_encode(&record,bytes)==0);
 CHECK(g19_hfi_record_decode(bytes,sizeof(bytes),&decoded)==0);
 CHECK(g19_hfi_record_encode(&decoded,again)==0&&memcmp(bytes,again,sizeof(bytes))==0);
 CHECK(record.polls==record.ready_polls+record.ack_polls&&!m.wrong&&m.bells<=1&&m.starts<=1);
}
static void fixture(const char *dir,const char *name){
 uint8_t bytes[G19_HFI_RECORD_BYTES];char path[1024];
 CHECK(g19_hfi_record_encode(&record,bytes)==0);
 CHECK(snprintf(path,sizeof(path),"%s/%s.bin",dir,name)>0);
 FILE *f=fopen(path,"wb");CHECK(f);CHECK(fwrite(bytes,1,sizeof(bytes),f)==sizeof(bytes));CHECK(fclose(f)==0);
}
int main(int argc,char **argv){
 CHECK(argc==2);
 CHECK(g19_hfi_crc32((const uint8_t *)"123456789",9)==0xcbf43926u);
 /* Every aligned ring position, both ready-before-init and explicit-init-ready,
  * shortest successful and longest accepted raw ACK. */
 for(unsigned at=0;at<GN_HFI_WORDS;at+=4)for(unsigned kind=0;kind<2;kind++){
  setup(at);m.pre_ready=kind;m.reply_words=kind?19:4;config.sequence=kind?4095:1;
  CHECK(run()==0&&record.stage==G19_HFI_COMPLETE&&record.raw_count==m.reply_words);
  CHECK(record.sent[0]==(config.sequence<<20|0x201u)&&record.sent[1]==0x20000000);
  CHECK(record.queue_first[0]==at&&record.queue_first[2]==at);
  CHECK(record.queue_last[0]==(at+4)%1024&&record.queue_last[2]==(at+((m.reply_words+3)&~3u))%1024);
  CHECK(record.raw[1]==record.sent[0]&&record.remote_status==0&&record.version==m.version);
  CHECK(record.start_calls==1&&record.doorbells==1&&record.stores==6&&record.q1_read_stores==1);
  roundtrip();
 }
 setup(1020);m.reply_words=19;CHECK(run()==0);roundtrip();fixture(argv[1],"success");
 {struct g19_hfi_record saved=record;CHECK(run()==GH_LIVE_STATE&&memcmp(&saved,&record,sizeof(record))==0);}
 /* All single-bit record corruption is rejected, including CRC/header bits. */
 {
  uint8_t bytes[320];struct g19_hfi_record out;CHECK(g19_hfi_record_encode(&record,bytes)==0);
  for(unsigned i=0;i<320;i++)for(unsigned bit=0;bit<8;bit++){
   bytes[i]^=(uint8_t)(1u<<bit);CHECK(g19_hfi_record_decode(bytes,320,&out)==G19_HFI_RECORD);bytes[i]^=(uint8_t)(1u<<bit);
  }
  CHECK(g19_hfi_record_decode(bytes,319,&out)==G19_HFI_RECORD);
 }
 setup(1020);m.remote=37;m.reply_words=3;CHECK(run()==GH_REMOTE_ERROR&&record.remote_status==37&&record.version==0);
 CHECK(record.q1_read_stores==1&&record.raw_count==3);roundtrip();fixture(argv[1],"remote");
 setup(1020);m.stale=0x100000;CHECK(run()==GN_STALE&&record.raw_count==4&&record.q1_read_stores==0);roundtrip();fixture(argv[1],"stale");
 for(unsigned words=0;words<=20;words++)if(words<4||words==20){
  setup(1020);m.reply_words=words;CHECK(run()==GN_PACKET&&record.q1_read_stores==0);roundtrip();
 }
 setup(0);m.reply_words=20;CHECK(run()==GN_PACKET);roundtrip();fixture(argv[1],"malformed");
 setup(0);m.version=0x10000000;CHECK(run()==GH_VERSION_MISMATCH&&record.q1_read_stores==0);roundtrip();
 setup(0);m.malformed_index=1;CHECK(run()==GN_QUEUE&&record.q1_read_stores==0&&record.raw_count==0);roundtrip();
 setup(0);m.reply_after=2;CHECK(run()==0&&m.delays==2&&record.ack_polls==3);roundtrip();
 setup(0);m.no_reply=1;config.poll_cap=4;m.step=0;CHECK(run()==G19_HFI_POLL_CAP&&record.polls==4&&record.q1_read_stores==0);roundtrip();
 setup(0);m.no_reply=1;config.timeout_ticks=12;CHECK(run()==G19_HFI_TIMEOUT&&record.last_tick-record.start_tick>=12);roundtrip();fixture(argv[1],"timeout");
 setup(0);m.pre_ready=0;m.never_ready=1;config.poll_cap=3;m.step=0;CHECK(run()==G19_HFI_POLL_CAP&&record.stage==G19_HFI_READY&&!record.stores);roundtrip();
 setup(0);m.expire_after_consume=1;CHECK(run()==G19_HFI_TIMEOUT&&record.q1_read_stores==1&&record.stage==G19_HFI_ACK);roundtrip();
 setup(0);m.rollback_at=3;CHECK(run()==G19_HFI_CLOCK&&!record.stores);roundtrip();
 setup(0);m.unknown=1;CHECK(run()==G19_HFI_HEALTH&&!m.starts&&record.last_health.helper_error==95);roundtrip();
 setup(0);m.reset=1;CHECK(run()==G19_HFI_HEALTH&&!m.starts);roundtrip();
 setup(0);m.fw=0;CHECK(run()==G19_HFI_HEALTH&&!m.starts);roundtrip();
 setup(0);m.mem[116/4]=4;CHECK(run()==GN_STALE&&!m.bells&&!record.stores);roundtrip();
 setup(0);m.mem[68/4]=4;CHECK(run()==GH_LIVE_STATE&&!m.bells&&!record.stores);roundtrip();
 setup(0);config.sequence=0;CHECK(run()==G19_HFI_ARGUMENT&&!m.events);roundtrip();
 setup(0);CHECK(run()==0);unsigned events=m.events,guards=m.guards;
 /* Every callback failure and lease loss in the actual successful trace. */
 for(unsigned failure=1;failure<=events;failure++){
  setup(0);m.fail=failure;CHECK(run()!=0);CHECK(record.error&&!m.wrong);roundtrip();
 }
 for(unsigned failure=1;failure<=guards;failure++){
  setup(0);m.lose=failure;CHECK(run()!=0);CHECK(record.error&&record.owner_failed);roundtrip();
 }
 setup(0);m.lose=1;CHECK(run()==G19_HFI_OWNERSHIP);roundtrip();fixture(argv[1],"owner");
 printf("G19_HFI_DIAGNOSTIC_HOST_PASS checks=%u alignedPositions=256 callbackFailures=%u ownerFailures=%u nativeAck=unproven\n",checks,events,guards);
 return 0;
}
