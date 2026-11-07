/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_hfi_live.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"line%d\n",__LINE__);exit(1);}}while(0)
static struct gh_live session;
struct model {uint32_t memory[GN_HFI_BYTES/4],events,fail,guards,lose,stores,doorbells,releases,acquires,wrong;}model;
static int event(void){return ++model.events!=model.fail;}
static int qualified(void*c,uint64_t epoch){CHECK(c==&model);return epoch==7&&++model.guards!=model.lose;}
static int read32(void*c,uint32_t at,uint32_t*out){CHECK(c==&model&&!(at&3)&&at<GN_HFI_BYTES);if(!event())return 0;*out=model.memory[at/4];return 1;}
static int write32(void*c,uint32_t at,uint32_t v){CHECK(c==&model&&!(at&3)&&at<GN_HFI_BYTES);if(!event())return 0;model.stores++;if(at==64||at==116)model.wrong++;CHECK(at==68||at==112||(at>=GN_HFI_QUEUE0&&at<GN_HFI_QUEUE0+4096));model.memory[at/4]=v;return 1;}
static int order(void*c,enum gh_live_order o){CHECK(c==&model&&(o==GH_LIVE_ACQUIRE||o==GH_LIVE_RELEASE));if(!event())return 0;if(o==GH_LIVE_RELEASE)model.releases++;else model.acquires++;return 1;}
static int bell(void*c){CHECK(c==&model);if(!event())return 0;CHECK(model.releases==1&&model.memory[68/4]%4==0);model.doorbells++;return 1;}
static struct gh_live_io io={&model,qualified,read32,write32,order,bell};
static void setup(unsigned at){memset(&model,0,sizeof(model));memset(&session,0,sizeof(session));session.epoch=7;session.gmu_address=0x60006000;CHECK(gn_hfi_init(model.memory,sizeof(model.memory),session.gmu_address)==GN_OK);model.memory[64/4]=model.memory[68/4]=at;model.memory[112/4]=model.memory[116/4]=at;}
static void response(unsigned words,unsigned status,unsigned version,unsigned stale){unsigned at=model.memory[112/4],off=(GN_HFI_QUEUE0+4096)/4;uint32_t data[4]={1u|(words<<8)|(1u<<16),session.sent^stale,status,version};for(unsigned i=0;i<words&&i<4;i++)model.memory[off+(at+i)%1024]=data[i];model.memory[116/4]=(at+((words+3)&~3u))%1024;}
int main(void){
#if G22_PREDECESSOR
 setup(0);CHECK(gh_live_request(&session,&io,1)==GN_OK);
 response(4,0,0x30020004,0);CHECK(gh_live_poll(&session,&io)==GH_VERSION_MISMATCH);
 CHECK(session.state==GH_LIVE_POISON&&model.memory[112/4]==0&&model.stores==5);
 printf("G22_PREDECESSOR_NATIVE_ACK_REFUSAL_PASS\n");return 0;
#endif
 for(unsigned at=0;at<1024;at+=4)for(unsigned seq=1;seq<=4095;seq+=17){setup(at);CHECK(gh_live_request(&session,&io,seq)==GN_OK);CHECK(session.state==GH_LIVE_PENDING&&model.doorbells==1&&model.memory[68/4]==(at+4)%1024);CHECK(model.memory[64/4]==at&&model.memory[112/4]==at);CHECK(gh_live_poll(&session,&io)==GN_EMPTY&&session.state==GH_LIVE_PENDING);response(4,0,0x30020004,0);CHECK(gh_live_poll(&session,&io)==GN_OK);CHECK(session.state==GH_LIVE_COMPLETE&&session.version==0x30020004&&!session.remote_status&&session.next_read==(at+4)%1024);CHECK(model.memory[116/4]==(at+4)%1024&&model.memory[64/4]==at&&!model.wrong&&model.stores==6&&model.releases==2);CHECK(gh_live_poll(&session,&io)==GH_LIVE_STATE);}
 setup(1020);CHECK(gh_live_request(&session,&io,4095)==GN_OK);response(3,99,0,0);CHECK(gh_live_poll(&session,&io)==GH_REMOTE_ERROR&&session.remote_status==99&&session.state==GH_LIVE_COMPLETE&&session.version==0);
 for(unsigned kind=0;kind<4;kind++){setup(1020);CHECK(gh_live_request(&session,&io,1)==GN_OK);response(kind==0?3:4,0,kind==1?0x10000000:0x20000000,kind==2?0x100000:0);if(kind==3)model.memory[116/4]=1023;int e=gh_live_poll(&session,&io);CHECK(e!=GN_OK&&session.state==GH_LIVE_POISON&&model.memory[112/4]==1020);}
 setup(0);CHECK(gh_live_request(&session,&io,1)==GN_OK);response(4,0,0x20000000,0);CHECK(gh_live_poll(&session,&io)==GN_OK);unsigned events=model.events,guards=model.guards;
 for(unsigned failure=1;failure<=events;failure++){setup(0);model.fail=failure;int e=gh_live_request(&session,&io,1);if(e==GN_OK){response(4,0,0x20000000,0);e=gh_live_poll(&session,&io);}CHECK(e!=GN_OK&&session.state==GH_LIVE_POISON&&!model.wrong);}
 for(unsigned failure=1;failure<=guards;failure++){setup(0);model.lose=failure;int e=gh_live_request(&session,&io,1);if(e==GN_OK){response(4,0,0x20000000,0);e=gh_live_poll(&session,&io);}CHECK(e!=GN_OK&&session.state==GH_LIVE_POISON&&!model.wrong);}
 for(unsigned at=0;at<24;at+=4){setup(0);model.memory[at/4]^=1;CHECK(gh_live_request(&session,&io,1)==GN_QUEUE&&!model.stores&&session.state==GH_LIVE_POISON);}
 setup(0);model.memory[28/4]^=4;CHECK(gh_live_request(&session,&io,1)==GN_QUEUE&&!model.stores);
 setup(0);model.memory[64/4]=4;CHECK(gh_live_request(&session,&io,1)==GN_FULL&&session.state==GH_LIVE_FRESH&&!model.stores);
 setup(0);CHECK(gh_live_request(&session,&io,0)==GN_ARG&&!model.stores);session.gmu_address=0x7ffff000;CHECK(gh_live_request(&session,&io,1)==GH_LIVE_OWNER&&!model.stores);
 printf("G22_NATIVE_ACK_REPLAY_PASS checks=%u allAlignedWrapPositions=256 staleMalformedAndEveryCallbackFailure=reject nativeQueueMapping=unproven nativeAck=unproven\n",checks);return 0;
}
