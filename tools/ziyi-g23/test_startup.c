/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_g23_startup.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line%u %s\n",(unsigned)__LINE__,#x);abort();}}while(0)
static struct {
 uint32_t mem[GN_HFI_BYTES/4],fault_at,calls,stores,bells,remote,stale,timeout,epoch_bad;
 uint64_t tick;struct gs_program program;struct gs_record record;
} model;
static unsigned checks;
static int pass(void){checks++;return ++model.calls!=model.fault_at;}
static int qualified(void*c,uint64_t e){(void)c;return pass()&&!model.epoch_bad&&e==0x11223344;}
static int read32(void*c,uint32_t at,uint32_t*v){(void)c;CHECK(!(at&3)&&at<GN_HFI_BYTES);if(!pass())return 0;*v=model.mem[at/4];return 1;}
static int write32(void*c,uint32_t at,uint32_t v){(void)c;CHECK(at==68||at==112||(at>=GN_HFI_QUEUE0&&at<GN_HFI_QUEUE0+4096));if(!pass())return 0;model.mem[at/4]=v;model.stores++;return 1;}
static int order(void*c,unsigned a){(void)c;CHECK(a==GS_RELEASE||a==GS_ACQUIRE);return pass();}
static int healthy(void*c){(void)c;return pass();}
static int now(void*c,uint64_t*t){(void)c;if(!pass())return 0;*t=model.tick;return 1;}
static int delay(void*c,uint32_t us){(void)c;CHECK(us==100);if(!pass())return 0;model.tick+=1920;return 1;}
static int bell(void*c,unsigned index,const struct gs_packet*p){
 (void)c;CHECK(index<5&&p==&model.program.packet[index]);if(!pass())return 0;model.bells++;
 uint32_t wr=model.mem[68/4],rd=model.mem[64/4],aligned=(p->count+3)&~3u;
 CHECK(wr==(rd+aligned)%1024);
 for(unsigned i=0;i<aligned;i++)CHECK(model.mem[GN_HFI_QUEUE0/4+(rd+i)%1024]==(i<p->count?p->words[i]:0xfafafafa));
 model.mem[64/4]=wr;
 if(model.timeout)return 1;
 uint32_t at=model.mem[116/4]; /* q1 write */
 model.mem[(GN_HFI_QUEUE0+4096)/4+at]=0x00010301;
 model.mem[(GN_HFI_QUEUE0+4096)/4+(at+1)%1024]=p->words[0]^(model.stale?0x100000:0);
 model.mem[(GN_HFI_QUEUE0+4096)/4+(at+2)%1024]=model.remote;
 model.mem[(GN_HFI_QUEUE0+4096)/4+(at+3)%1024]=0xfafafafa;
 model.mem[116/4]=(at+4)%1024;return 1;
}
static const struct gs_io io={NULL,qualified,read32,write32,order,bell,healthy,now,delay};
static void setup(const struct gr_resources*r,unsigned start){
 memset(&model,0,sizeof(model));CHECK(gs_prepare(&model.program,r,0x11223344,0x60006000,2136718)==0);
 CHECK(gn_hfi_init(model.mem,sizeof(model.mem),0x60006000)==0);
 model.mem[64/4]=model.mem[68/4]=model.mem[112/4]=model.mem[116/4]=start;
 model.record.state=GS_PREPARED;model.record.epoch=model.program.epoch;model.record.found=127;model.record.resource_crc=gs_crc((const uint8_t*)r,sizeof(*r));
 model.record.header_equal=model.record.resources_equal=1;model.tick=100;
}
int main(int argc,char**argv){
 CHECK(argc==3);uint8_t data[2248];FILE*f=fopen(argv[1],"rb");CHECK(f);CHECK(fread(data,1,sizeof(data),f)==sizeof(data));fclose(f);
 struct gr_resources r={0};CHECK(gr_parse(data,sizeof(data),&r)==0);uint32_t baseline_calls=0;uint8_t raw[768],copy[768];
 for(unsigned start=0;start<1024;start+=4){
  setup(&r,start);CHECK(gs_run(&model.program,&io,19200000,&model.record)==0);CHECK(model.record.state==GS_COMPLETE&&model.record.acks==5&&model.bells==5);
  CHECK(gs_encode(&model.record,raw)==0);struct gs_record decoded={0};CHECK(gs_decode(raw,768,&decoded)==0&&gs_encode(&decoded,copy)==0&&!memcmp(raw,copy,768));
  for(unsigned i=0;i<5;i++)CHECK(model.record.step[i].raw_count==3&&model.record.step[i].raw[1]==model.program.packet[i].words[0]&&model.record.step[i].complete==1);
  baseline_calls=model.calls;
 }
 f=fopen(argv[2],"wb");CHECK(f);CHECK(fwrite(raw,1,768,f)==768);fclose(f);
 unsigned faults=0;
 for(unsigned at=1;at<=baseline_calls;at++){
  setup(&r,4);model.fault_at=at;CHECK(gs_run(&model.program,&io,19200000,&model.record)!=0);CHECK(model.record.state==GS_POISON);struct gs_record partial={0};CHECK(gs_encode(&model.record,copy)==0&&gs_decode(copy,768,&partial)==0);unsigned bells=model.bells,stores=model.stores;CHECK(gs_run(&model.program,&io,19200000,&model.record)==GS_STATE&&model.bells==bells&&model.stores==stores);faults++;
 }
 setup(&r,4);model.remote=1;CHECK(gs_run(&model.program,&io,19200000,&model.record)==GS_REMOTE&&model.record.acks==0&&model.record.step[0].raw[2]==1);
 setup(&r,4);model.stale=1;CHECK(gs_run(&model.program,&io,19200000,&model.record)==GS_PACKET&&model.mem[112/4]==4);
 setup(&r,4);model.timeout=1;CHECK(gs_run(&model.program,&io,19200000,&model.record)==GS_TIMEOUT&&model.tick<=96002020&&model.bells==1);
 setup(&r,4);model.epoch_bad=1;CHECK(gs_run(&model.program,&io,19200000,&model.record)==GS_OWNER&&model.bells==0&&model.stores==0);
 setup(&r,4);model.mem[3]=13;CHECK(gs_run(&model.program,&io,19200000,&model.record)==GS_QUEUE&&model.record.queue_key==0&&model.record.queue_seen==15&&model.record.queue_values[3]==13);CHECK(gs_encode(&model.record,copy)==0);
 setup(&r,4);model.mem[72/4]=3;CHECK(gs_run(&model.program,&io,19200000,&model.record)==GS_QUEUE&&model.record.queue_key==3&&model.record.queue_seen==63&&model.record.queue_values[0]==3);CHECK(gs_encode(&model.record,copy)==0);
 for(unsigned cmd=0;cmd<5;cmd++)for(unsigned word=0;word<GS_PACKET_WORDS;word++){
  setup(&r,4);model.program.packet[cmd].words[word]^=1;CHECK(gs_run(&model.program,&io,19200000,&model.record)!=0&&model.bells==0&&model.stores==0);
 }
 for(unsigned bit=0;bit<768*8;bit++){memcpy(copy,raw,768);copy[bit/8]^=(uint8_t)(1u<<(bit&7));struct gs_record bad;CHECK(gs_decode(copy,768,&bad)!=0);}
 printf("G23_STARTUP_BATCH_HOST_PASS positions256 callbackFailures%u recordBitControls6144 checks%u native0\n",faults,checks);return 0;
}
