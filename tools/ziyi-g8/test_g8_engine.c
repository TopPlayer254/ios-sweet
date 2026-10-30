/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_g8_fwinit.h"
#include "ziyi_g7_sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static struct g8_workspace w;
static unsigned checks,test;
struct model {uint64_t regs[G8_REGS],ticks;uint32_t tcm[8192],initial[8192],ack;unsigned locked,reads,writes,forbidden,fail_read,fail_write,race_write,timeout,missing_ack,fw_reads,bad_proof,bad_owned,cache_fail,physical_fail,pa_calls,cache_calls,corrupt_backup,pending,release_fail,release_ack_race,release_stuck,fsr_runtime;uint8_t trusted_sha[32];};
static struct model m;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"G8 line%u test%u phase%u err%u poison%u reads%u writes%u at%llx\n",__LINE__,test,w.record.phase,w.record.error,w.record.poison,w.record.reads,w.record.writes,(unsigned long long)w.record.last_address);exit(1);}}while(0)
static int idx(uint64_t at,unsigned width){for(unsigned i=0;i<G8_REGS;i++)if(g8_regs[i].address==at&&g8_regs[i].width==width)return (int)i;return -1;}
static uint64_t raw(uint64_t at,unsigned width){
 if(at==0x3d9953c)return 0x80001fe0;if(at==0x3d9905c)return 0x22f001;if(at==0x3d7e600)return 0;
 if(at==GD_ACK)return m.missing_ack?0:m.ack;
 if(at==0x3db2058)return m.regs[36]|(m.regs[25]&1?0x400u:0u);
 if(at==0x3da0000)return 0x210406;if(at==0x3da0020)return 0x4c017e09;if(at==0x3da0024)return 0x30000007;if(at==0x3da0028)return 0x5511;
 if(at>=0x3da0804&&at<0x3da0818)return 0;if(at>=0x3da0c04&&at<0x3da0c18)return 0x20000;
 if(at>=0x3d6d000&&at<0x3d75000)return m.tcm[(at-0x3d6d000)/4];
 if(at==0x3d7e648)return (w.record.phase==G8_CLEAR)?m.pending:0;
 if(at==0x3d8ec14||at==0x3d8ec4c||at==0x3db27f4)return 0;
 int i=idx(at,width);if(i>=0)return m.regs[i];m.forbidden++;return 0;
}
static uint64_t rd(void*arg,uint64_t at,unsigned width,int*ok){(void)arg;m.reads++;m.ticks++;*ok=1;if(!g8_read_allowed(&w,at,width)){m.forbidden++;*ok=0;return 0;}if(m.fail_read==m.reads){*ok=0;return 0xdeadbeef;}if(at==0x3d7e070&&w.record.phase==G8_WAIT&&!m.timeout&&++m.fw_reads==3){m.regs[3]=0x100;m.regs[36]|=m.fsr_runtime;}return raw(at,width);}
static void wr(void*arg,uint64_t at,unsigned width,uint64_t value,int*ok){(void)arg;*ok=0;m.writes++;if(m.race_write==m.writes){m.regs[19]=0;}
 uint64_t observe=0;int mode=g8_observation_address(at,width,&observe);uint32_t pending=0;if(w.record.phase==G8_CONFIG_RESTORE&&(at==0x3d7e64c||at==0x3d8ec18))pending=raw(at==0x3d7e64c?0x3d7e648:0x3d8ec14,4);
 if(w.record.phase==G9_RELEASE_BUS&&at==GD_HALT){if(m.release_ack_race)m.ack^=1;pending=raw(GD_ACK,4);}
 if(mode<0||!g8_store_guard(&w,at,width,value,mode?raw(observe,width):0,(uint32_t)raw(GD_RESET,4),pending)){m.forbidden++;return;}if(m.fail_write==m.writes)return;
 if(at>=0x3d6d000&&at<0x3d75000)m.tcm[(at-0x3d6d000)/4]=(uint32_t)value;
 else if(at==0x3d7e644)m.pending&=~(uint32_t)value;
 else if(at==0x3db2058){if(value&0x600u){m.forbidden++;return;}m.regs[36]&=~value;}
 else if(at==0x3db2610||at==0x3db27f0){}
 else {int i=idx(at,width);if(i<0){m.forbidden++;return;}m.regs[i]=value;if(at==GD_HALT&&!(m.release_stuck&&w.record.phase==G9_RELEASE_BUS))m.ack=value?m.ack|(uint32_t)value:0;}
 if(m.corrupt_backup&&w.record.phase==G8_START)w.backup[0][31]^=1;
 *ok=1;
}
static uint64_t ticks(void*a){(void)a;return m.ticks;}
static void delay(void*a,uint32_t us){(void)a;m.ticks+=us*20;}
static void barrier(void*a){(void)a;}
static int lease(void*a,uint64_t epoch,int acquire){(void)a;if(epoch!=1)return 0;if(acquire){if(m.locked)return 0;m.locked=1;return 1;}if(m.release_fail)return 0;if(!m.locked)return 0;m.locked=0;return 1;}
static int proof(void*a,const struct g8_prereq*q,const struct g8_blob*b){(void)a;return !m.bad_proof&&q->epoch==1&&b->board==0x06060201&&!memcmp(b->firmware_sha256,m.trusted_sha,32);}
static int owned(void*a,const void*p,size_t n){(void)a;return !m.bad_owned&&p==&w&&n==sizeof(w);}
static uint64_t pa(void*a,const void*p){(void)a;if(++m.pa_calls==m.physical_fail)return 0;uintptr_t at=(uintptr_t)p;if(at>=(uintptr_t)w.memory.backing&&at<(uintptr_t)w.memory.backing+sizeof(w.memory.backing))return 0x900000000ULL+((at-(uintptr_t)w.memory.backing)/4096)*0x8000;if(at>=(uintptr_t)w.memory.tables&&at<(uintptr_t)w.memory.tables+sizeof(w.memory.tables))return 0x910000000ULL+((at-(uintptr_t)w.memory.tables)/4096)*0x4000;return 0;}
static int cache(void*a,void*p,size_t n){(void)a;(void)p;(void)n;return ++m.cache_calls!=m.cache_fail;}
static void u32(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static unsigned block(unsigned at,uint32_t va,uint32_t size,uint32_t extent){u32(w.firmware+at,va);u32(w.firmware+at+4,size);u32(w.firmware+at+8,1);u32(w.firmware+at+12,extent);if(size)for(unsigned i=0;i<size;i++)w.firmware[at+16+i]=(uint8_t)(at+i);return at+16+size;}
static void setup(struct g8_prereq*q,struct g8_io*io){memset(&w,0,sizeof(w));memset(&m,0,sizeof(m));m.ticks=1;m.regs[19]=1;m.regs[35]=0x20000;for(unsigned i=0;i<8192;i++)m.initial[i]=m.tcm[i]=i*2654435761u^0x1badcafe;
 unsigned at=0;const uint32_t meta[11][2]={{0,0x4000},{0x10004000,0x4000},{0x4000,0xe000},{0x100000,0x9000},{0x200000,0x13000},{0x300000,0x7000},{0x60000000,0x1000},{0x60001000,0x1000},{0,4},{4,4},{8,4}};for(unsigned i=0;i<11;i++)at=block(at,meta[i][0],0,meta[i][1]);at=block(at,0,0x4000,0);unsigned dt=at+16;at=block(at,0x10004000,0x4000,0);at=block(at,0x4000,0x4000,0);at=block(at,0x100000,G8_FW_BYTES-240-3*0x4000,0);CHECK(at==G8_FW_BYTES);const uint32_t versions[]={0x30020004,0xffffffff,0x30010002,0,0x20010000};for(unsigned i=0;i<5;i++)u32(w.firmware+dt+0x3fe0+4*i,versions[i]);
 w.blob.bytes=G8_FW_BYTES;w.blob.board=0x06060201;g7_sha256(w.firmware,G8_FW_BYTES,w.blob.firmware_sha256);memcpy(m.trusted_sha,w.blob.firmware_sha256,32);
 *q=(struct g8_prereq){.epoch=1,.board=0x06060201,.hlos_cb=6,.hlos_smr=6,.rates={19200144,150000460,199999956,150000460},.current_el=4,.sctlr=0x1005,.acd_off=1,.qdss_off=1,.lm_bcl_off=1,.hfi_off=1,.inherited_ddr_probe=1};
 *io=(struct g8_io){0,rd,wr,ticks,delay,barrier,lease,proof,owned,{0,pa,cache,cache,barrier}};
}
int main(void){struct g8_prereq q;struct g8_io io;setup(&q,&io);CHECK(g8_run(&w,&q,&io));CHECK(!m.locked&&!m.forbidden&&w.record.restored&&w.record.lease_released&&!memcmp(m.initial,m.tcm,sizeof(m.tcm)));CHECK(w.record.drain.client_ack&&w.record.drain.arb_ack);unsigned totalr=m.reads,totalw=m.writes;
 for(unsigned halt=2;halt<=3;halt++){setup(&q,&io);m.regs[23]=halt;m.ack=3;CHECK(g8_run(&w,&q,&io));CHECK(w.record.initial_halt==halt&&w.record.initial_ack==3&&w.record.bus_released&&w.record.final_ack==3&&m.regs[23]==halt&&m.ack==3&&w.record.restored&&w.record.lease_released&&!m.forbidden);}
 setup(&q,&io);m.regs[23]=0;m.ack=3;CHECK(g8_run(&w,&q,&io));CHECK(w.record.initial_halt==0&&w.record.initial_ack==3&&w.record.bus_observations==2&&w.record.bus_released&&w.record.final_ack==0&&m.regs[23]==0&&m.ack==0&&w.record.restored&&!m.forbidden);
 const unsigned rejected[][2]={{0,1},{0,2},{1,0},{1,1},{1,3},{2,0},{2,1},{2,2},{3,0},{3,1},{3,2},{4,3},{2,7}};
 for(unsigned i=0;i<sizeof(rejected)/sizeof(rejected[0]);i++){setup(&q,&io);m.regs[23]=rejected[i][0];m.ack=rejected[i][1];CHECK(!g8_run(&w,&q,&io));CHECK(!m.writes&&w.record.lease_released&&!w.record.retain_allocations);}

 setup(&q,&io);m.regs[25]=1;CHECK(g8_run(&w,&q,&io));CHECK(w.record.restored&&m.regs[25]==1&&!m.forbidden&&w.record.old[36]==0x400);
 const unsigned invalid_fsr[]={1,2,0x200,0x600,0x800,0x40000000u,0x80000000u};
 for(unsigned i=0;i<sizeof(invalid_fsr)/sizeof(invalid_fsr[0]);i++){setup(&q,&io);m.regs[36]=invalid_fsr[i];CHECK(!g8_run(&w,&q,&io));CHECK(!m.writes&&w.record.lease_released);}
 setup(&q,&io);m.fsr_runtime=2;CHECK(!g8_run(&w,&q,&io));CHECK(w.record.error==G8_DEVICE_FAULT&&w.record.restored&&w.record.lease_released&&!m.regs[36]&&!m.forbidden);
 setup(&q,&io);m.fsr_runtime=0x800;CHECK(!g8_run(&w,&q,&io));CHECK(w.record.poison&&w.record.retain_allocations&&m.locked);
 setup(&q,&io);m.regs[23]=2;m.ack=3;m.release_ack_race=1;CHECK(!g8_run(&w,&q,&io));CHECK(w.record.poison&&w.record.retain_allocations&&!(w.record.flags&G8_RELEASED_RESET)&&m.regs[23]==2&&m.forbidden==1);
 setup(&q,&io);m.regs[23]=2;m.ack=3;m.release_stuck=1;CHECK(!g8_run(&w,&q,&io));CHECK(w.record.error==G8_TIMEOUT&&w.record.poison&&w.record.retain_allocations&&!(w.record.flags&G8_RELEASED_RESET)&&!m.forbidden);

 for(test=1;test<=18;test++){setup(&q,&io);if(test==1)m.timeout=1;if(test==2)m.missing_ack=1;if(test==3)m.bad_proof=1;if(test==4)m.bad_owned=1;if(test==5)m.regs[3]=0x100;if(test==6)m.regs[34]=0x80000005;if(test==7)m.regs[26]=1;if(test==8)m.regs[36]=1;if(test==9)w.firmware[300]^=1;if(test==10)m.corrupt_backup=1;if(test==11)m.pending=0x80000000;if(test==12)m.release_fail=1;if(test==13)m.cache_fail=1;if(test==14)m.physical_fail=40;if(test==15)q.hfi_off=0;if(test==16)q.hlos_cb=7;if(test==17)q.sctlr=0;if(test==18)m.regs[27]=0x10;CHECK(!g8_run(&w,&q,&io));if(test==1)CHECK(w.record.restored&&w.record.lease_released&&!(w.record.flags&G8_FW100));if(test==2||test==10||test==11)CHECK(w.record.retain_allocations&&!w.record.lease_released&&m.locked);if(test==3||test==4||test==9||test==15||test==16||test==17)CHECK(!m.reads&&!m.writes);}
 const unsigned positions[]={1,2,50,100,500,8000,16000,16400,16500,17000,20000,30000};for(test=0;test<sizeof(positions)/sizeof(positions[0]);test++){if(positions[test]>totalr)continue;setup(&q,&io);m.fail_read=positions[test];CHECK(!g8_run(&w,&q,&io));CHECK(w.record.poison&&m.reads==positions[test]);}
 const unsigned stores[]={1,2,5,10,20,40,100,4000,8192,10000,16000};for(test=0;test<sizeof(stores)/sizeof(stores[0]);test++){if(stores[test]>totalw)continue;setup(&q,&io);m.fail_write=stores[test];CHECK(!g8_run(&w,&q,&io));CHECK(w.record.poison&&m.writes==stores[test]&&w.record.retain_allocations&&m.locked);setup(&q,&io);m.race_write=stores[test];CHECK(!g8_run(&w,&q,&io));CHECK(w.record.poison&&m.writes==stores[test]&&m.locked);}
 setup(&q,&io);m.pending=2;CHECK(!g8_run(&w,&q,&io));CHECK(w.record.restored&&w.record.lease_released&&!m.pending&&w.record.error==G8_DEVICE_FAULT);printf("G10_SYNTHETIC_ENGINE_PASS checks=%u positiveReads=%u positiveWrites=%u fullTCMRestore=1 inheritedHaltAck=0/3,2/3,3/3 idleAckNormalized=0 FSRformat=qualified releaseRace=reject releaseTimeout=retain\n",checks,totalr,totalw);return 0;}
