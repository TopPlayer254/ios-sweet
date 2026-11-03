/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_g8_fwinit.h"
#include "ziyi_g7_sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static struct g8_workspace w;
static unsigned checks,test;
struct model {uint64_t regs[G8_REGS],ticks;uint32_t tcm[8192],initial[8192],ack;unsigned locked,reads,writes,forbidden,fail_read,fail_write,race_write,timeout,missing_ack,fw_reads,bad_proof,bad_owned,cache_fail,physical_fail,pa_calls,cache_calls,corrupt_backup,pending,release_fail,release_ack_race,release_stuck,fsr_runtime,format_flip,tcr_flip,rebase_race,rebase_reads,rebase_route,sctlr_no_private,interframe_no_sptp,lowpower_writes,hyst_config_writes;uint8_t trusted_sha[32];};
static struct model m;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"G8 line%u test%u phase%u err%u poison%u reads%u writes%u at%llx\n",__LINE__,test,w.record.phase,w.record.error,w.record.poison,w.record.reads,w.record.writes,(unsigned long long)w.record.last_address);exit(1);}}while(0)
static int idx(uint64_t at,unsigned width){for(unsigned i=0;i<G8_REGS;i++)if(g8_regs[i].address==at&&g8_regs[i].width==width)return (int)i;return -1;}
static uint64_t raw(uint64_t at,unsigned width){
 if(at==0x3d9953c)return 0x80001fe0;if(at==0x3d9905c)return 0x22f001;if(at==0x3d7e600)return 0;
 if(at==GD_ACK)return m.missing_ack?0:m.ack;
 if(at==0x3d7e300&&m.interframe_no_sptp)return m.regs[15]&~4u;
 if(at==0x3db2000&&m.sctlr_no_private)return m.regs[26]&~0x1000u;
 if(at==0x3db2010)return m.regs[28]|(m.regs[25]&1?0x60u:0u);
 if(at==0x3db2058)return m.regs[36]|(m.regs[25]&1?0x400u:0u);
 if(at==0x3da0000)return 0x210406;if(at==0x3da0020)return 0x4c017e09;if(at==0x3da0024)return 0x30000007;if(at==0x3da0028)return 0x5511;
 if(at>=0x3da0804&&at<0x3da0818)return 0;if(at>=0x3da0c04&&at<0x3da0c18)return 0x20000;
 if(at>=0x3d6d000&&at<0x3d75000)return m.tcm[(at-0x3d6d000)/4];
 if(at==0x3d7e648)return (w.record.phase==G8_CLEAR)?m.pending:0;
 if(at==0x3d8ec14||at==0x3d8ec4c||at==0x3db27f4)return 0;
 int i=idx(at,width);if(i>=0)return m.regs[i];m.forbidden++;return 0;
}
static uint64_t rd(void*arg,uint64_t at,unsigned width,int*ok){(void)arg;m.reads++;m.ticks++;*ok=1;if(!g8_read_allowed(&w,at,width)){m.forbidden++;*ok=0;return 0;}if(m.fail_read==m.reads){*ok=0;return 0xdeadbeef;}if(at==0x3d7e070&&w.record.phase==G8_WAIT&&!m.timeout&&++m.fw_reads==3){m.regs[3]=0x100;m.regs[36]|=m.fsr_runtime;}if((w.record.flags&G13_CONTEXT_REINIT)&&!(w.record.flags&G13_CONTEXT_REBASED)&&w.record.phase==G8_INSTALL&&w.record.expected[24]==0x1f300){
  if(m.rebase_route&&at==0x3da0800)return 0x84000005;
  if(m.rebase_race&&at==g8_regs[28].address&&++m.rebase_reads==2)return raw(at,width)^1;
 }return raw(at,width);}
static void wr(void*arg,uint64_t at,unsigned width,uint64_t value,int*ok){(void)arg;*ok=0;m.writes++;if(m.race_write==m.writes){m.regs[19]=0;}
 if(at==GD_HALT&&(value&~3u)){m.forbidden++;return;}
 uint64_t observe=0;int mode=g8_observation_address(at,width,&observe);uint32_t pending=0;if(w.record.phase==G8_CONFIG_RESTORE&&(at==0x3d7e64c||at==0x3d8ec18))pending=raw(at==0x3d7e64c?0x3d7e648:0x3d8ec14,4);
 if(w.record.phase==G9_RELEASE_BUS&&at==GD_HALT){if(m.release_ack_race)m.ack^=1;pending=raw(GD_ACK,4);}
 if(mode<0||!g8_store_guard(&w,at,width,value,mode?raw(observe,width):0,(uint32_t)raw(GD_RESET,4),pending)){m.forbidden++;return;}if(m.fail_write==m.writes)return;
 if(at>=0x3d6d000&&at<0x3d75000)m.tcm[(at-0x3d6d000)/4]=(uint32_t)value;
 else if(at==0x3d7e644)m.pending&=~(uint32_t)value;
 else if(at==0x3db2058){if(value&0x600u){m.forbidden++;return;}m.regs[36]&=~value;}
 else if(at==0x3db2610||at==0x3db27f0){}
 else {int i=idx(at,width);if(i<0){m.forbidden++;return;}
 if((i==28||i==31||i==32||i==33)&&m.regs[24]!=0x1f300){m.forbidden++;return;}
 if(w.record.phase==G8_CONFIG&&i==15&&value!=0x9c40400)m.lowpower_writes++;
 if(w.record.phase==G8_CONFIG&&(i==16||i==17))m.hyst_config_writes++;
 m.regs[i]=value;
 if(i==24&&value==0x1f300&&m.format_flip){for(unsigned j=27;j<=33;j++)m.regs[j]=0;m.format_flip=0;}
 if(i==28&&m.tcr_flip){m.regs[30]=0xfe000000deadb000ULL;m.regs[31]=0xdc00000012345000ULL;m.tcr_flip=0;}
if(at==GD_HALT&&!(m.release_stuck&&w.record.phase==G9_RELEASE_BUS))m.ack=value?m.ack|(uint32_t)value:0;}
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
static void setup(struct g8_prereq*q,struct g8_io*io){memset(&w,0,sizeof(w));memset(&m,0,sizeof(m));m.ticks=1;m.regs[19]=1;m.regs[24]=0x1f300;m.regs[25]=1;m.regs[35]=0x20000;for(unsigned i=0;i<8192;i++)m.initial[i]=m.tcm[i]=i*2654435761u^0x1badcafe;
 unsigned at=0;const uint32_t meta[11][2]={{0,0x4000},{0x10004000,0x4000},{0x4000,0xe000},{0x100000,0x9000},{0x200000,0x13000},{0x300000,0x7000},{0x60000000,0x1000},{0x60001000,0x1000},{0,4},{4,4},{8,4}};for(unsigned i=0;i<11;i++)at=block(at,meta[i][0],0,meta[i][1]);at=block(at,0,0x4000,0);unsigned dt=at+16;at=block(at,0x10004000,0x4000,0);at=block(at,0x4000,0x4000,0);at=block(at,0x100000,G8_FW_BYTES-240-3*0x4000,0);CHECK(at==G8_FW_BYTES);const uint32_t versions[]={0x30020004,0xffffffff,0x30010002,0,0x20010000};for(unsigned i=0;i<5;i++)u32(w.firmware+dt+0x3fe0+4*i,versions[i]);
 w.blob.bytes=G8_FW_BYTES;w.blob.board=0x06060201;g7_sha256(w.firmware,G8_FW_BYTES,w.blob.firmware_sha256);memcpy(m.trusted_sha,w.blob.firmware_sha256,32);
 *q=(struct g8_prereq){.epoch=1,.board=0x06060201,.hlos_cb=6,.hlos_smr=6,.rates={19200144,150000460,199999956,150000460},.current_el=4,.sctlr=0x1005,.acd_off=1,.qdss_off=1,.lm_bcl_off=1,.hfi_off=1,.inherited_ddr_probe=1};
 *io=(struct g8_io){0,rd,wr,ticks,delay,barrier,lease,proof,owned,{0,pa,cache,cache,barrier}};
}
static void unused_bank(void){
 m.regs[24]=m.regs[25]=0;m.regs[26]=0x8332714;m.regs[27]=3;
 m.regs[28]=0x61;m.regs[29]=0x4071545;m.regs[30]=0xa01021c182b4210ULL;
 m.regs[31]=0xf6aca022c064f06ULL;m.regs[32]=0x532681cc;m.regs[33]=0x766a0a00;
 m.regs[15]=0x9c40402;m.regs[16]=0xa2710;m.regs[17]=0xa2710;m.regs[23]=8;m.ack=3;m.format_flip=1;m.tcr_flip=1;m.sctlr_no_private=1;m.interframe_no_sptp=1;
}
#ifdef G15_G14_NEGATIVE_CONTROL
int main(void){struct g8_prereq q;struct g8_io io;setup(&q,&io);unused_bank();
 CHECK(!g8_run(&w,&q,&io));CHECK(w.record.phase==G8_INSTALL&&w.record.error==G8_OWNER&&w.record.poison&&w.record.writes==13&&w.record.reads==16674);
 CHECK(w.record.last_address==0x3db2000&&w.record.last_value==0xc00027&&w.record.pending_value==0xc01027);
 CHECK((w.record.flags&G13_CONTEXT_REBASED)&&(w.record.flags&G13_CONTEXT_TCR_READY)&&!(w.record.flags&G8_LOADED)&&w.record.retain_allocations&&m.locked);
 printf("G15_ACTUAL_G14_REGRESSION_CONTROL_PASS checks=%u writes13 readbackc00027 pendingc01027 noFirmware=1\n",checks);return 0;}
#else
int main(void){struct g8_prereq q;struct g8_io io;setup(&q,&io);CHECK(g8_run(&w,&q,&io));CHECK(!m.locked&&!m.forbidden&&w.record.restored&&w.record.lease_released&&!memcmp(m.initial,m.tcm,sizeof(m.tcm)));CHECK(w.record.drain.client_ack&&w.record.drain.arb_ack);unsigned totalr=m.reads,totalw=m.writes;
 for(unsigned halt=2;halt<=3;halt++){setup(&q,&io);m.regs[23]=halt;m.ack=3;CHECK(g8_run(&w,&q,&io));CHECK(w.record.initial_halt==halt&&w.record.initial_ack==3&&w.record.bus_released&&w.record.final_ack==3&&m.regs[23]==halt&&m.ack==3&&w.record.restored&&w.record.lease_released&&!m.forbidden);}
 setup(&q,&io);m.regs[23]=0;m.ack=3;CHECK(g8_run(&w,&q,&io));CHECK(w.record.initial_halt==0&&w.record.initial_ack==3&&w.record.bus_observations==2&&w.record.bus_released&&w.record.final_ack==0&&m.regs[23]==0&&m.ack==0&&w.record.restored&&!m.forbidden);
 setup(&q,&io);m.regs[23]=8;m.ack=3;CHECK(g8_run(&w,&q,&io));CHECK(w.record.old[23]==8&&w.record.initial_halt==8&&w.record.initial_ack==3&&w.record.bus_observations==2&&w.record.bus_released&&m.regs[23]==0&&m.ack==0&&!w.record.restored&&!(w.record.flags&G8_RESTORED)&&(w.record.flags&G12_NORMALIZED_CLEANUP)&&w.record.lease_released&&!m.locked&&!m.forbidden&&!memcmp(m.initial,m.tcm,sizeof(m.tcm)));
 for(unsigned i=0;i<G8_REGS;i++)if(i!=23)CHECK(raw(g8_regs[i].address,g8_regs[i].width)==w.record.old[i]);
 w.record.lease_released=0;w.record.phase=G8_CONFIG_RESTORE;w.record.flags&=~(G12_BUS_NORMALIZED|G12_NORMALIZED_CLEANUP);w.record.pending_address=GD_HALT;w.record.pending_width=4;w.record.pending_value=8;w.record.expected[23]=8;CHECK(!g8_store_guard(&w,GD_HALT,4,8,8,1,0));
 {const unsigned points[]={1,100,8000,17000,60000};for(unsigned i=0;i<5;i++){setup(&q,&io);m.regs[23]=8;m.ack=3;m.fail_read=points[i];CHECK(!g8_run(&w,&q,&io));CHECK(w.record.poison&&m.reads==points[i]&&!m.forbidden);}}
 {const unsigned points[]={1,20,8000,16380};for(unsigned i=0;i<4;i++){setup(&q,&io);m.regs[23]=8;m.ack=3;m.fail_write=points[i];CHECK(!g8_run(&w,&q,&io));CHECK(w.record.poison&&w.record.retain_allocations&&m.writes==points[i]&&!m.forbidden);}}
 setup(&q,&io);m.regs[23]=8;m.ack=3;m.release_ack_race=1;CHECK(!g8_run(&w,&q,&io));CHECK(w.record.poison&&w.record.retain_allocations&&!(w.record.flags&G8_RELEASED_RESET)&&m.regs[23]==8);
 setup(&q,&io);m.regs[23]=8;m.ack=3;m.release_stuck=1;CHECK(!g8_run(&w,&q,&io));CHECK(w.record.poison&&w.record.retain_allocations&&!(w.record.flags&G8_RELEASED_RESET)&&!(w.record.flags&G12_NORMALIZED_CLEANUP));
 setup(&q,&io);m.regs[23]=8;m.ack=3;m.timeout=1;CHECK(!g8_run(&w,&q,&io));CHECK(w.record.error==G8_TIMEOUT&&!w.record.restored&&(w.record.flags&G12_NORMALIZED_CLEANUP)&&w.record.lease_released&&!m.locked);

 /* Unassigned Stage2/AArch32 bank: unknown/reserved values are retained as
  * evidence, never replayed into Stage1. Model a real format-dependent read
  * change on CBAR programming, then require the fresh rebase observations. */
 setup(&q,&io);unused_bank();CHECK(g8_run(&w,&q,&io));
 CHECK((w.record.flags&(G13_CONTEXT_REINIT|G13_CONTEXT_REBASED|G13_CONTEXT_TCR_READY|G13_CONTEXT_CLEANUP))==(G13_CONTEXT_REINIT|G13_CONTEXT_REBASED|G13_CONTEXT_TCR_READY|G13_CONTEXT_CLEANUP));
 CHECK((w.record.flags&G12_NORMALIZED_CLEANUP)&&!w.record.restored&&!(w.record.flags&G8_RESTORED)&&!m.locked&&!m.forbidden&&w.record.lease_released);
 CHECK(w.record.old[31]==0xf6aca022c064f06ULL&&m.regs[24]==0x1f300&&m.regs[25]==1&&!m.regs[26]&&!m.regs[30]&&!m.regs[31]&&m.regs[35]==0x20000);
 CHECK(m.regs[15]==0x9c40402&&m.regs[16]==0xa2710&&m.regs[17]==0xa2710&&!memcmp(m.initial,m.tcm,sizeof(m.tcm)));
 CHECK(!g8_invalid_registers(&w));
 w.record.lease_released=0;w.record.phase=G8_CONTEXT_RESTORE;w.record.pending_address=g8_regs[31].address;w.record.pending_width=8;w.record.pending_value=w.record.old[31];
 CHECK(!g8_store_guard(&w,g8_regs[31].address,8,w.record.old[31],0,1,0));
 setup(&q,&io);unused_bank();m.rebase_race=1;CHECK(!g8_run(&w,&q,&io));CHECK(w.record.poison&&w.record.retain_allocations&&m.locked&&!(w.record.flags&G13_CONTEXT_REBASED)&&!(w.record.flags&G8_LOADED));
 setup(&q,&io);unused_bank();m.rebase_route=1;CHECK(!g8_run(&w,&q,&io));CHECK(w.record.poison&&w.record.retain_allocations&&m.locked&&!(w.record.flags&G8_LOADED));
 setup(&q,&io);unused_bank();m.regs[26]|=1;CHECK(!g8_run(&w,&q,&io));CHECK(!m.writes&&w.record.lease_released&&!(w.record.flags&G13_CONTEXT_REINIT));
 setup(&q,&io);unused_bank();m.timeout=1;CHECK(!g8_run(&w,&q,&io));CHECK(w.record.error==G8_TIMEOUT&&(w.record.flags&G13_CONTEXT_CLEANUP)&&(w.record.flags&G12_NORMALIZED_CLEANUP)&&!w.record.restored&&w.record.lease_released&&!m.forbidden);
 {const uint32_t counts[]={0,0xffffffffu,0xa2710,0xffff0000u,0xffffu};for(unsigned i=0;i<5;i++){setup(&q,&io);m.regs[16]=counts[i];m.regs[17]=counts[4-i];CHECK(g8_run(&w,&q,&io));CHECK(m.regs[16]==counts[i]&&m.regs[17]==counts[4-i]&&!m.forbidden);}}
 for(unsigned bits=0;bits<8;bits++){setup(&q,&io);m.regs[15]=0x9c40400|bits;CHECK(g8_run(&w,&q,&io));CHECK(m.regs[15]==(0x9c40400|bits)&&!m.forbidden);}
 setup(&q,&io);m.regs[15]=0x9c40408;CHECK(!g8_run(&w,&q,&io));CHECK(!m.writes&&w.record.lease_released);
 setup(&q,&io);m.regs[16]=0x100000000ULL;CHECK(!g8_run(&w,&q,&io));CHECK(!m.writes&&w.record.lease_released);
 setup(&q,&io);CHECK(g8_regs[28].mask==0x3807f&&g8_regs[28].literal==0x38071);CHECK(g8_run(&w,&q,&io));CHECK(raw(0x3db2010,4)==w.record.old[28]);
 setup(&q,&io);m.regs[28]=0x80;CHECK(!g8_run(&w,&q,&io));CHECK(!m.writes&&w.record.lease_released);
 setup(&q,&io);unused_bank();CHECK(g8_regs[26].mask==0x0fdf11f7&&g8_regs[26].literal==0xc00027);CHECK(g8_run(&w,&q,&io));
 CHECK((w.record.flags&G8_FW100)&&(w.record.flags&G13_CONTEXT_CLEANUP)&&!w.record.poison&&w.record.lease_released);
 /* Actual route/store predicates still require the finite complete SCTLR. */
 w.record.lease_released=0;w.record.phase=G8_INSTALL;w.record.expected[26]=0xc00027;w.record.expected[35]=0x302;
 w.record.pending_address=g8_regs[34].address;w.record.pending_width=4;w.record.pending_value=0x84000005;
 CHECK(g8_store_guard(&w,g8_regs[34].address,4,0x84000005,w.record.expected[34],1,0));
 w.record.expected[26]=0xc01027;CHECK(!g8_store_guard(&w,g8_regs[34].address,4,0x84000005,w.record.expected[34],1,0));
 w.record.expected[26]=0;w.record.pending_address=g8_regs[26].address;w.record.pending_value=0xc01027;
 CHECK(!g8_store_guard(&w,g8_regs[26].address,4,0xc01027,0,1,0));
 w.record.pending_value=0xc00027;CHECK(g8_store_guard(&w,g8_regs[26].address,4,0xc00027,0,1,0));
 setup(&q,&io);unused_bank();CHECK(g8_run(&w,&q,&io));CHECK(!m.lowpower_writes&&!m.hyst_config_writes);
 CHECK((w.record.flags&G8_FW100)&&(w.record.flags&G13_CONTEXT_CLEANUP)&&w.record.lease_released);
 w.record.lease_released=0;w.record.phase=G8_CONFIG;w.record.pending_address=g8_regs[15].address;w.record.pending_width=4;w.record.pending_value=0x9c40403;
 CHECK(!g8_store_guard(&w,g8_regs[15].address,4,0x9c40403,w.record.expected[15],1,0));
 w.record.pending_value=0x9c40407;CHECK(!g8_store_guard(&w,g8_regs[15].address,4,0x9c40407,w.record.expected[15],1,0));
 w.record.pending_value=0x9c40400;CHECK(g8_store_guard(&w,g8_regs[15].address,4,0x9c40400,w.record.expected[15],1,0));
 for(unsigned i=16;i<=17;i++){w.record.pending_address=g8_regs[i].address;w.record.pending_value=g8_regs[i].literal;CHECK(!g8_store_guard(&w,g8_regs[i].address,4,g8_regs[i].literal,w.record.expected[i],1,0));}
 const unsigned rejected[][2]={{8,0},{8,1},{8,2},{8,7},{9,3},{10,3},{11,3},{0x80000008u,3},{0,1},{0,2},{1,0},{1,1},{1,3},{2,0},{2,1},{2,2},{3,0},{3,1},{3,2},{4,3},{2,7}};
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
 setup(&q,&io);m.pending=2;CHECK(!g8_run(&w,&q,&io));CHECK(w.record.restored&&w.record.lease_released&&!m.pending&&w.record.error==G8_DEVICE_FAULT);printf("G16_SYNTHETIC_ENGINE_PASS checks=%u positiveReads=%u positiveWrites=%u fullTCMRestore=1 inheritedHaltAck=0/3,2/3,3/3 exact8/3NormalizedNoRawRestore=1 contextFormatRebase=stable normalizedContextCleanupNoRawRestore=1 idleAckNormalized=0 FSRformat=qualified releaseRace=reject releaseTimeout=retain\n",checks,totalr,totalw);return 0;}

#endif
