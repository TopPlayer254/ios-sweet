/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_g8_drain.h"
static int bad(struct gd_record*r,unsigned code){if(!r->error)r->error=code;r->poison=1;return 0;}
static uint32_t rd(struct gd_record*r,const struct gd_io*io,uint64_t address){if(r->poison)return 0;int ok=0;r->last_address=address;uint32_t value=io->read(io->ctx,address,&ok);r->reads++;r->last_value=value;if(!ok)bad(r,1);return value;}
int gd_store_guard(const struct gd_record*r,uint64_t address,uint32_t value,uint32_t now){return r&&r->epoch&&!r->poison&&address==GD_HALT&&((r->phase==1&&value==1&&now==0)||(r->phase==2&&r->client_ack&&value==2&&now==1));}
static int put(struct gd_record*r,const struct gd_io*io,uint32_t value){
 if(rd(r,io,GD_RESET)!=1||r->poison)return bad(r,2);
 uint32_t now=rd(r,io,GD_HALT);if(r->poison)return 0;
 if(!gd_store_guard(r,GD_HALT,value,now))return bad(r,3);
 int ok=0;io->write(io->ctx,GD_HALT,value,&ok);r->stores++;if(!ok)return bad(r,1);
 if(rd(r,io,GD_HALT)!=value||r->poison)return bad(r,3);return 1;
}
static int wait_ack(struct gd_record*r,const struct gd_io*io,uint32_t bit,unsigned timeout){
 for(unsigned i=0;i<timeout;i++){
  if(rd(r,io,GD_RESET)!=1||r->poison)return bad(r,2);
  uint32_t ack=rd(r,io,GD_ACK);if(r->poison)return 0;if(ack&~3u)return bad(r,3);
  if(ack&bit)return 1;io->delay(io->ctx,1);
 }
 return bad(r,4);
}
int gd_drain(struct gd_record*r,uint64_t epoch,unsigned timeout,const struct gd_io*io){
 if(!r||!io||!io->read||!io->write||!io->delay||!epoch||!timeout||timeout>100000)return 0;
 *r=(struct gd_record){.epoch=epoch};
 if(rd(r,io,GD_RESET)!=1||r->poison)return bad(r,2);
 if(rd(r,io,GD_HALT)||rd(r,io,GD_ACK)||r->poison)return bad(r,3);
 r->phase=1;if(!put(r,io,1)||!wait_ack(r,io,1,timeout))return 0;r->client_ack=1;
 /* Source a6xx_halt_gbif discards its first ret; this implementation never
  * issues ARB halt after a missing CLIENT acknowledgement. */
 if(rd(r,io,GD_ACK)&2||r->poison)return bad(r,3);
 r->phase=2;if(!put(r,io,2)||!wait_ack(r,io,2,timeout))return 0;r->arb_ack=1;r->phase=3;
 return 1;
}
