#include "ziyi_g8_fwinit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct zg_state {unsigned gmu_enabled,lease;uint64_t lease_epoch;struct g8_workspace firmware;};
static struct zg_state s;
static unsigned checks,stores,pointers,good_adt;static uint32_t cm3,halt,ack;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"line%d\n",__LINE__);exit(1);}}while(0)
#include "ziyi_g8_engine.c"
static int zg_adt_check(struct zg_state*p,int first){CHECK(p==&s&&!first);return good_adt;}
static uint64_t zg_g8_load(void*p,uint64_t at,unsigned width,int*ok){CHECK(p==&s&&width==4);*ok=1;if(at==GD_RESET)return cm3;if(at==GD_HALT)return halt;if(at==GD_ACK)return ack;*ok=0;return 0;}
static void*zg_g8_pointer(struct zg_state*p,uint64_t at,unsigned width,int writable){CHECK(p==&s&&at==GD_RESET&&width==4&&writable);pointers++;return &cm3;}
static void zg_barrier(void*p){CHECK(p==&s);stores++;}
static void zg_g8_write(void*ctx,uint64_t at,unsigned width,uint64_t value,int*ok){struct zg_state*s=ctx;*ok=0;
 if(!s->gmu_enabled||!zg_adt_check(s,0)||__atomic_load_n(&s->lease,__ATOMIC_ACQUIRE)!=1||s->lease_epoch!=s->firmware.record.epoch)return;
 uint64_t observe=0;int mode=g8_observation_address(at,width,&observe),good=1;if(mode<0)return;uint64_t now=mode?zg_g8_load(s,observe,width,&good):0;if(!good)return;
 uint32_t reset=(uint32_t)zg_g8_load(s,GD_RESET,4,&good);if(!good)return;uint32_t pending=0;
 if(s->firmware.record.phase==G8_CONFIG_RESTORE&&(at==0x3d7e64c||at==0x3d8ec18)){pending=(uint32_t)zg_g8_load(s,at==0x3d7e64c?0x3d7e648:0x3d8ec14,4,&good);if(!good)return;}
 if(s->firmware.record.phase==G9_RELEASE_BUS&&at==GD_HALT){pending=(uint32_t)zg_g8_load(s,GD_ACK,4,&good);if(!good)return;}
 /* Fresh native HALT/ACK observations immediately before CM3 store. */
 if(s->firmware.record.phase==G8_START&&at==GD_RESET){uint32_t halt=(uint32_t)zg_g8_load(s,GD_HALT,4,&good);if(!good||halt)return;pending=(uint32_t)zg_g8_load(s,GD_ACK,4,&good);if(!good)return;}
 if(!g8_store_guard(&s->firmware,at,width,value,now,reset,pending))return;void*p=zg_g8_pointer(s,at,width,1);if(!p)return;
 if(width==8)*(volatile uint64_t*)p=value;else *(volatile uint32_t*)p=(uint32_t)value;zg_barrier(s);*ok=1;
}

static void setup(unsigned delayed){memset(&s,0,sizeof(s));s.gmu_enabled=s.lease=good_adt=1;s.lease_epoch=9;cm3=1;halt=0;ack=delayed?3:0;stores=pointers=0;struct g8_record*r=&s.firmware.record;r->epoch=9;r->phase=G8_START;r->flags=G8_LEASE|G8_SAVED|G8_SCOPED|G8_PUBLISHED|G8_MAPPED|G8_LOADED|G8_HELD_RESET;if(delayed)r->flags|=G18_VENDOR_HANDOFF;r->old[23]=8;r->saved_crc=old_crc(r);r->initial_halt=8;r->initial_ack=3;r->bus_observations=2;r->final_ack=ack;r->bus_released=!delayed;r->expected[19]=1;r->pending_address=GD_RESET;r->pending_width=4;}
int main(void){int ok;for(unsigned delayed=0;delayed<2;delayed++){setup(delayed);zg_g8_write(&s,GD_RESET,4,0,&ok);CHECK(ok&&cm3==0&&stores==1&&pointers==1);}
for(unsigned fault=0;fault<15;fault++){setup(1);struct g8_record*r=&s.firmware.record;switch(fault){case 0:ack=0;break;case 1:ack=1;break;case 2:ack=2;break;case 3:ack=7;break;case 4:halt=1;break;case 5:cm3=0;break;case 6:good_adt=0;break;case 7:s.lease=0;break;case 8:s.lease_epoch++;break;case 9:r->flags&=~G8_LOADED;break;case 10:r->flags&=~G18_VENDOR_HANDOFF;break;case 11:r->initial_halt=0;break;case 12:r->initial_ack=0;break;case 13:r->bus_observations=1;break;case 14:r->poison=1;break;}zg_g8_write(&s,GD_RESET,4,0,&ok);CHECK(!ok&&!stores&&!pointers);}
printf("G18_ACTUAL_NATIVE_CALLBACK_HOST_PASS checks=%u freshACKandHALTrequired=1 uncertainStores=0 nativeMMIO=0\n",checks);return 0;}
