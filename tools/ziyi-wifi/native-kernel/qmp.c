/* SPDX-License-Identifier: GPL-2.0-only */
#include "native.h"
static const uint32_t owned[4]={0x10,0x24,0x2c,0x18};
static int bad(struct zwn_qmp*q,unsigned error,unsigned poison){if(!q->error)q->error=error;q->poison|=poison;return 0;}
static int auth(struct zwn_qmp*q,const struct zwn_io*io){return q->epoch&&!q->poison&&io->authority(io->ctx,q->epoch,q->operation);}
static int state(uint32_t v){return v==ZWN_UP||v==ZWN_DOWN;}
static uint32_t rd(struct zwn_qmp*q,const struct zwn_io*io,uint32_t off){
 q->last_offset=off;if((off&3)||off>ZWN_QMP_BYTES-4||!auth(q,io)){bad(q,ZWN_OWNER,1);return 0;}
 int ok=0;uint32_t v=io->read32(io->ctx,ZWN_QMP+off,&ok);q->reads++;q->last_value=v;if(!ok)bad(q,ZWN_IO,1);return v;
}
static int unchanged(struct zwn_qmp*q,const struct zwn_io*io){
 const unsigned fixed[]={0,1,2,7,8,13,14};
 for(unsigned n=0;n<sizeof(fixed)/sizeof(fixed[0]);n++){
  unsigned i=fixed[n];if(rd(q,io,i*4)!=q->descriptor[i]||q->poison)return bad(q,ZWN_OWNER,1);
 }
 for(unsigned i=0;i<4;i++)if(rd(q,io,owned[i])!=q->expected[i]||q->poison)return bad(q,ZWN_OWNER,1);
 return 1;
}
int zwn_qmp_write_allowed(const struct zwn_qmp*q,uint64_t at,uint32_t value,uint32_t now){
 if(!q||!q->epoch||q->poison)return 0;
 if(at==ZWN_IPCC+0xc)return value==0&&(q->operation==ZWN_QMP_OPEN||q->operation==ZWN_QMP_MESSAGE||q->operation==ZWN_QMP_CLOSE);
 if(at<ZWN_QMP||at>ZWN_QMP+ZWN_QMP_BYTES-4||(at&3))return 0;uint32_t off=(uint32_t)(at-ZWN_QMP);
 for(unsigned i=0;i<4;i++)if(off==owned[i]){
  if(now!=q->expected[i])return 0;
  if(q->operation==ZWN_QMP_OPEN)return state(value);
  return q->operation==ZWN_QMP_CLOSE&&!q->inherited&&(q->changed&(1u<<i))&&value==q->original[i];
 }
 if(q->operation!=ZWN_QMP_MESSAGE||!q->open||!q->mailbox||q->send_pending)return 0;
 if(off==q->mailbox)return now==0&&value==64;
 return off>=q->mailbox+4&&off<q->mailbox+68;
}
static int wr(struct zwn_qmp*q,const struct zwn_io*io,uint32_t off,uint32_t value){
 if(!unchanged(q,io))return 0;uint32_t now=rd(q,io,off);if(q->poison||!auth(q,io)||!zwn_qmp_write_allowed(q,ZWN_QMP+off,value,now))return bad(q,ZWN_OWNER,1);
 int ok=0;io->write32(io->ctx,ZWN_QMP+off,value,&ok);q->writes++;io->barrier(io->ctx);if(!ok)return bad(q,ZWN_IO,1);
 for(unsigned i=0;i<4;i++)if(off==owned[i]){q->expected[i]=value;q->changed|=1u<<i;}
 /* AOP can consume message length immediately; don't demand that64 survives. */
 if(off!=q->mailbox&&rd(q,io,off)!=value)return bad(q,ZWN_OWNER,1);return !q->poison;
}
static int kick(struct zwn_qmp*q,const struct zwn_io*io){
 if(!unchanged(q,io)||!auth(q,io)||!zwn_qmp_write_allowed(q,ZWN_IPCC+0xc,0,0))return bad(q,ZWN_OWNER,1);
 int ok=0;io->barrier(io->ctx);io->write32(io->ctx,ZWN_IPCC+0xc,0,&ok);q->writes++;q->kicks++;io->barrier(io->ctx);return ok?1:bad(q,ZWN_IO,1);
}
static int wait(struct zwn_qmp*q,const struct zwn_io*io,uint32_t off,uint32_t value){
 for(unsigned i=0;i<1000;i++){uint32_t got=rd(q,io,off);if(q->poison)return 0;if(got==value)return 1;io->delay_us(io->ctx,1000);}
 return bad(q,ZWN_TIMEOUT,1);
}
int zwn_qmp_open(struct zwn_qmp*q,uint64_t epoch,const struct zwn_io*io){
 if(!q||!io||!io->read32||!io->write32||!io->authority||!io->barrier||!io->delay_us||!epoch||q->epoch)return 0;
 *q=(struct zwn_qmp){.epoch=epoch,.operation=ZWN_QMP_OPEN};
 for(unsigned i=0;i<15;i++){q->descriptor[i]=rd(q,io,i*4);if(q->poison)return 0;}
 if(q->descriptor[0]!=0x4d41494c||q->descriptor[1]!=1)return bad(q,ZWN_FORMAT,0);
 q->mailbox=q->descriptor[14];q->size=q->descriptor[13];uint32_t other=q->descriptor[8],size=q->descriptor[7];
 if(q->mailbox<60||(q->mailbox&3)||q->size<68||(q->size&3)||q->mailbox>ZWN_QMP_BYTES||q->size>ZWN_QMP_BYTES-q->mailbox||
    other<60||(other&3)||size<4||(size&3)||other>ZWN_QMP_BYTES||size>ZWN_QMP_BYTES-other||
    (q->mailbox<other+size&&other<q->mailbox+q->size))return bad(q,ZWN_FORMAT,0);
 for(unsigned i=3;i<=12;i++)if(i!=7&&i!=8&&!state(q->descriptor[i]))return bad(q,ZWN_FORMAT,0);
 for(unsigned i=0;i<4;i++)q->original[i]=q->expected[i]=q->descriptor[owned[i]/4];
 if(!unchanged(q,io))return 0;
 if(rd(q,io,q->mailbox)||q->poison)return bad(q,ZWN_BUSY,0);
 q->inherited=q->descriptor[3]==ZWN_UP&&q->descriptor[4]==ZWN_UP&&q->descriptor[5]==ZWN_UP&&q->descriptor[6]==ZWN_UP&&
 q->descriptor[9]==ZWN_UP&&q->descriptor[10]==ZWN_UP&&q->descriptor[11]==ZWN_UP&&q->descriptor[12]==ZWN_UP;
 if(q->inherited){q->open=1;return 1;}
 if(q->descriptor[3]!=ZWN_UP)return bad(q,ZWN_BUSY,0);
 if(!wr(q,io,0x10,ZWN_UP)||!wr(q,io,0x24,ZWN_UP)||!kick(q,io)||!wait(q,io,0x28,ZWN_UP)||
 !wr(q,io,0x2c,ZWN_UP)||!kick(q,io)||!wait(q,io,0x14,ZWN_UP)||!wr(q,io,0x18,ZWN_UP)||!kick(q,io)||!wait(q,io,0x30,ZWN_UP))return 0;
 q->open=1;return 1;
}
int zwn_qmp_send_image(struct zwn_qmp*q,int on,const struct zwn_io*io){
 if(!q||!io||q->poison||q->error||!q->open||q->send_pending||(on!=0&&on!=1))return 0;q->operation=ZWN_QMP_MESSAGE;
 const char*text=on?"{class: image, res: load_state, name: wpss, val: on}":"{class: image, res: load_state, name: wpss, val: off}";
 uint32_t words[16]={0};for(unsigned i=0;text[i];i++){if(i>=63)return bad(q,ZWN_FORMAT,0);words[i/4]|=(uint32_t)(uint8_t)text[i]<<(8*(i&3));}
 if(!unchanged(q,io)||rd(q,io,q->mailbox)||q->poison)return bad(q,ZWN_BUSY,0);
 for(unsigned i=0;i<16;i++)if(!wr(q,io,q->mailbox+4+4*i,words[i]))return 0;
 if(!wr(q,io,q->mailbox,64))return 0;q->send_pending=1;q->sent++;
 if(!kick(q,io)||!wait(q,io,q->mailbox,0))return 0;q->send_pending=0;q->acknowledged++;return 1;
}
int zwn_qmp_close(struct zwn_qmp*q,const struct zwn_io*io){
 if(!q||!io||q->poison||q->error||q->send_pending||!q->open)return 0;
 if(q->inherited)return 1;q->operation=ZWN_QMP_CLOSE;
 for(unsigned i=4;i-->0;)if(q->changed&(1u<<i)){if(!wr(q,io,owned[i],q->original[i]))return 0;}
 if(!kick(q,io))return 0;q->open=0;return 1;
}
