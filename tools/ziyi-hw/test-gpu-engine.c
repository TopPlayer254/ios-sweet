/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_g7_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks,test;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"G7 failure line%u test%u err%u phase%u reads%u\n",__LINE__,test,r.error,r.phase,r.reads);exit(1);}}while(0)
struct model {struct g7_record *record;uint64_t ticks;unsigned reads,locked,acquires,releases,barriers,bad_buffer,fail_acquire,fail_release,forbidden,fail_read,tcm_reads,change_tcm,active_route,bad_s2cr,active_cb,pending_fault,reset_glitch,changed_control,control_reads;uint64_t failed_address;};
static uint8_t copy0[G7_TCM_BYTES+32],copy1[G7_TCM_BYTES+32];
static uint32_t read_io(void *arg,uint64_t a,int*ok){
 struct model*m=arg;m->reads++;m->ticks++;*ok=1;
 if(!g7_read_allowed(m->record,a)){m->forbidden++;*ok=0;return 0;}
 if(m->fail_read==m->reads){m->failed_address=a;*ok=0;return 0xdeadbeef;}
 if(a==0x03d9953c)return 0x80001fe0;
 if(a==0x03d9905c)return 0x22f001;
 if(a==0x03d7e000)return m->reset_glitch&&m->tcm_reads>=1024?0:1;
 if(a==0x03d7e600||a==0x03d7e070)return 0;
 if(a>=0x03da0000&&a<0x03dc0000){
  uint32_t off=(uint32_t)(a-0x03da0000);
  if(off==0)return 0x210406;if(off==0x20)return 0x4c017e09;if(off==0x24)return 0x30000007;if(off==0x28)return 0x5511;
  if(off>=0x800&&off<0x818)return m->active_route&&off==0x800?0x80000005:0;
  if(off>=0xc00&&off<0xc18)return m->bad_s2cr&&off==0xc00?0:0x20000;
  if(off==0x12000)return m->active_cb?1:0;if(off==0x12058)return m->pending_fault?1:0;
  return 0;
 }
 if(a>=0x03d6d000&&a<0x03d75000){
  uint32_t off=(uint32_t)(a-0x03d6d000),value=(off/4)*2654435761u^0x5a3c217bu;m->tcm_reads++;
  if(m->change_tcm&&m->tcm_reads>8192&&off==0x5abc)value^=1;return value;
 }
 if(m->record->phase==4){m->control_reads++;if(m->changed_control&&m->control_reads>G7_CONTROLS)return 1;}
 return 0;
}
static uint64_t ticks(void*arg){return ((struct model*)arg)->ticks;}
static int lease(void*arg,uint64_t epoch,int acquire){struct model*m=arg;if(epoch!=1)return 0;if(acquire){m->acquires++;if(m->fail_acquire||m->locked)return 0;m->locked=1;return 1;}m->releases++;if(m->fail_release||!m->locked)return 0;m->locked=0;return 1;}
static int owned(void*arg,const void*p,size_t bytes){struct model*m=arg;return !m->bad_buffer&&bytes==G7_TCM_BYTES&&(p==copy0+16||p==copy1+16);}
static void barrier(void*arg){((struct model*)arg)->barriers++;}
static void setup(struct model*m,struct g7_record*r,struct g7_prereq*q,struct g7_buffers*b,struct g7_io*io){
 memset(m,0,sizeof(*m));memset(r,0,sizeof(*r));memset(copy0,0x69,sizeof(copy0));memset(copy1,0x96,sizeof(copy1));m->record=r;m->ticks=1;
 *q=(struct g7_prereq){.epoch=1,.board=G7_BOARD,.hlos_cb=6,.hlos_smr=6,.contract_kind=G7_CONTRACT_LIVE_FDT,.rates={19200144,150000460,199999956,150000460},.mapped_device_io=1,.execution_kind=G7_EXEC_FLAT_OFF,.current_el=4};
 *b=(struct g7_buffers){copy0+16,copy1+16,G7_TCM_BYTES,G7_TCM_BYTES};*io=(struct g7_io){m,read_io,ticks,lease,owned,barrier};
}
int main(int argc,char**argv){
 struct model m;struct g7_record r;struct g7_prereq q;struct g7_buffers b;struct g7_io io;
 setup(&m,&r,&q,&b,&io);CHECK(g7_run_acquire(&r,&q,&b,&io));CHECK(r.flags==511&&r.tcm_equal&&!r.poison&&!r.error&&!r.device_writes&&m.tcm_reads==16384&&m.barriers==2&&m.acquires==1&&m.releases==1&&!m.locked&&!m.forbidden);
 CHECK(r.tcm_crc[0]==r.tcm_crc[1]&&!memcmp(r.tcm_sha256[0],r.tcm_sha256[1],32));
 CHECK(r.plan.cb==2&&r.plan.smr==0&&r.plan.software_context_map==7&&!r.plan.hardware_write_authority&&!g7_firmware_ready(&r));
 for(unsigned i=0;i<16;i++){CHECK(copy0[i]==0x69&&copy1[i]==0x96);CHECK(copy0[G7_TCM_BYTES+16+i]==0x69&&copy1[G7_TCM_BYTES+16+i]==0x96);}
 unsigned total=m.reads;
 if(argc==2){FILE*f=fopen(argv[1],"wbx");CHECK(f&&fwrite(&r,1,sizeof(r),f)==sizeof(r));CHECK(!fclose(f));}
 for(test=1;test<=total;test++){
  setup(&m,&r,&q,&b,&io);m.fail_read=test;CHECK(!g7_run_acquire(&r,&q,&b,&io));CHECK(r.error==G7_IO&&r.poison&&r.reads==test&&m.reads==test&&r.last_address==m.failed_address&&r.last_value==0xdeadbeef&&m.releases==1&&!m.forbidden&&!r.device_writes);
 }
 for(test=0;test<8;test++){
  setup(&m,&r,&q,&b,&io);
  if(test==0)q.board^=1;if(test==1)q.hlos_cb=7;if(test==2)q.hlos_smr=9;if(test==3)q.contract_kind=0;if(test==4)q.handoff_pairs=1;if(test==5)q.rates[2]=0;if(test==6)q.mapped_device_io=0;if(test==7)q.epoch=2;
  CHECK(!g7_run_acquire(&r,&q,&b,&io));CHECK(!m.reads&&!m.acquires&&!m.releases);
 }
 setup(&m,&r,&q,&b,&io);test=100001;m.bad_buffer=1;CHECK(!g7_run_acquire(&r,&q,&b,&io));CHECK(r.error==G7_BUFFER&&!m.acquires);
 setup(&m,&r,&q,&b,&io);test=100002;b.copy1=b.copy0+4;CHECK(!g7_run_acquire(&r,&q,&b,&io));CHECK(r.error==G7_BUFFER&&!m.acquires);
 setup(&m,&r,&q,&b,&io);test=100003;m.fail_acquire=1;CHECK(!g7_run_acquire(&r,&q,&b,&io));CHECK(r.error==G7_LEASE&&!m.reads&&!m.releases);
 setup(&m,&r,&q,&b,&io);test=100004;m.fail_release=1;CHECK(!g7_run_acquire(&r,&q,&b,&io));CHECK(r.error==G7_LEASE&&r.poison&&m.locked);
 setup(&m,&r,&q,&b,&io);test=100005;m.active_route=1;CHECK(!g7_run_acquire(&r,&q,&b,&io));CHECK(r.error==G7_ACTIVE&&r.poison&&!m.tcm_reads);
 setup(&m,&r,&q,&b,&io);test=100006;m.bad_s2cr=1;CHECK(!g7_run_acquire(&r,&q,&b,&io));CHECK(r.error==G7_ACTIVE&&r.poison&&!m.tcm_reads);
 setup(&m,&r,&q,&b,&io);test=100007;m.active_cb=1;CHECK(!g7_run_acquire(&r,&q,&b,&io));CHECK(r.error==G7_ACTIVE&&r.poison&&!m.tcm_reads);
 setup(&m,&r,&q,&b,&io);test=100008;m.pending_fault=1;CHECK(!g7_run_acquire(&r,&q,&b,&io));CHECK(r.error==G7_ACTIVE&&r.poison&&!m.tcm_reads);
 setup(&m,&r,&q,&b,&io);test=100009;m.reset_glitch=1;CHECK(!g7_run_acquire(&r,&q,&b,&io));CHECK(r.error==G7_PREREQ&&r.poison&&m.tcm_reads==1024);
 setup(&m,&r,&q,&b,&io);test=100010;m.change_tcm=1;CHECK(!g7_run_acquire(&r,&q,&b,&io));CHECK(r.error==G7_CHANGED&&r.poison&&!r.tcm_equal);
 setup(&m,&r,&q,&b,&io);test=100011;m.changed_control=1;CHECK(!g7_run_acquire(&r,&q,&b,&io));CHECK(r.error==G7_CHANGED&&r.poison&&!m.tcm_reads);
 setup(&m,&r,&q,&b,&io);test=100012;CHECK(!g7_run_acquire((struct g7_record*)&io,&q,&b,&io));CHECK(!m.acquires&&!m.reads);
 setup(&m,&r,&q,&b,&io);test=100013;CHECK(!g7_run_acquire((struct g7_record*)&q,&q,&b,&io));CHECK(!m.acquires&&!m.reads);
 setup(&m,&r,&q,&b,&io);test=100014;q.contract_kind=G7_CONTRACT_XNU_SECURE_DT;q.execution_kind=G7_EXEC_KERNEL_ON;q.sctlr=0x1005;CHECK(g7_run_acquire(&r,&q,&b,&io));
 setup(&m,&r,&q,&b,&io);test=100015;q.contract_kind=G7_CONTRACT_XNU_SECURE_DT;q.execution_kind=G7_EXEC_KERNEL_ON;CHECK(!g7_run_acquire(&r,&q,&b,&io));CHECK(r.error==G7_PREREQ&&!m.reads);
 setup(&m,&r,&q,&b,&io);test=100016;q.sctlr=0x1005;CHECK(!g7_run_acquire(&r,&q,&b,&io));CHECK(r.error==G7_PREREQ&&!m.reads);
 setup(&m,&r,&q,&b,&io);test=100017;q.current_el=8;CHECK(!g7_run_acquire(&r,&q,&b,&io));CHECK(r.error==G7_PREREQ&&!m.reads);
 setup(&m,&r,&q,&b,&io);CHECK(g7_run_acquire(&r,&q,&b,&io));
 for(test=0;test<0x20000;test+=4){uint64_t addr=0x03da0000ull+test;int expect=test==0||test==0x20||test==0x24||test==0x28||(test>=0x800&&test<0x818)||(test>=0xc00&&test<0xc18)||test==0x1008||test==0x1808;const uint32_t words[]={0,4,0x10,0x20,0x24,0x28,0x2c,0x30,0x38,0x3c,0x58,0x60,0x64,0x68,0x6c};for(unsigned i=0;i<15;i++)if(test==0x12000+words[i])expect=1;CHECK(g7_read_allowed(&r,addr)==expect);}
 r.plan.cb=3;CHECK(!g7_read_allowed(&r,0x03db3000)&&!g7_read_allowed(&r,0x03db2000));r.plan.cb=2;r.plan.hardware_write_authority=1;CHECK(!g7_read_allowed(&r,0x03db2000)&&!g7_firmware_ready(&r));
 printf("G7_ACQUISITION_TCM_ENGINE_PASS checks=%u faultPositions=%u TCMbytes=%u firmwareAuthority=0 nativeExecuted=0\n",checks,total,G7_TCM_BYTES);return 0;
}
