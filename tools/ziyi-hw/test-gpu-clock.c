/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_g7_clock.h"
#include <stdio.h>
#include <string.h>
_Static_assert(G6_COLLAPSE_SETTLE_US==101u,"source1us+100us collapse interval");
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"G2 line %u case %u error %u phase %u flags %x writes %u poison %u restore %u\n",__LINE__,test,r.error,r.phase,r.flags,m.writes,r.poison,r.restore);return 1;}}while(0)
struct model {
 uint32_t values[G2_NREGS],initial[G2_NREGS],pending[G2_NREGS],status,count;
 uint64_t us,finish;unsigned reads,writes,forbidden,ahb,measure_running,smmu_reads;
 unsigned fail_write,fail_read,owner,stale,never,off_ahb,active_gmu,bad_layout,bad_counter,changed_backup,gx_active;
 unsigned power_timeout,update_stuck,ao_reads,phase_reads[100];
 unsigned corrupt_backup,guard_reject,bad_power_status,observed_initial;
 unsigned dirty_missing,dirty_stuck,clear_immediate,cmd_owner_poll,transient_clear,cmd_samples,dirty_forge_checks;
 unsigned dirty_delayed,dirty_delay[G2_NREGS],dirty_store_race,dirty_late;
 unsigned clock_never,unrequested_stores;
 uint64_t cx_settle_at,cx_enable_at;
 unsigned early_enable_reads,enable_settles,skip_enable_wait;
 unsigned early_collapse_reads,early_collapse_writes,collapse_settles,collapse_unknown,collapse_never_off,collapse_preunknown;
 unsigned physical_matches,physical_contexts;
 unsigned hook_calls,hook_fail,hook_writes;
 struct g7c_record *r;
};
static int index_of(uint64_t b,uint32_t o) {
 for(unsigned i=0;i<G2_NREGS;i++)if(g7c_regs[i].base==b&&g7c_regs[i].offset==o)return (int)i;
 return -1;
}
static uint32_t source_rate(struct model *m,unsigned cfg) {
 uint32_t v=m->values[cfg],source=(v>>8)&7,div=(v&31)+1;
 if(v&(1u<<20))return 0; /* plain RCG must leave inherited HW control */
 if(source==0)return 38400000/div;
 if(source==6&&(m->values[G2_PLL_VOTE]&0x10000))return 600000000/div;
 return 0;
}
static uint32_t rate(struct model *m) {
 if(m->values[G2_CX_POWER]&1)return 0;
 switch(m->values[G2_GPU_MUX]&255) {
 case 0x1f:return (m->values[G2_CXO]&1)?19200000:0;
 case 0x16:return m->ahb&&!m->off_ahb?source_rate(m,G2_HUB_CFG)/4:0;
 case 0x1e:return (m->values[G2_CX_GMU]&1)?source_rate(m,G2_GMU_CFG):0;
 case 0x2f:return (m->values[G2_HUB_AON]&1)?source_rate(m,G2_HUB_CFG):0;
 default:return 0;
 }
}
static uint32_t rd(void *arg,uint64_t b,uint32_t o,int *ok) {
 struct model *m=arg;m->reads++;*ok=1;
 if(m->cx_settle_at&&m->us<m->cx_settle_at){m->early_collapse_reads++;*ok=0;return 0x5022f001;}
 if(m->cx_enable_at&&m->us<m->cx_enable_at){m->early_enable_reads++;return 0x5022f000;}
 if(m->r->phase<100&&!m->phase_reads[m->r->phase])m->phase_reads[m->r->phase]=m->reads;
 if(!g7c_read_allowed(m->r,b,o)){m->forbidden++;*ok=0;return 0;}
 if(m->fail_read==m->reads){*ok=0;return 0;}
 int i=index_of(b,o);
 if(i>=0) {
  if(m->dirty_delay[i]&&!--m->dirty_delay[i])m->values[i]|=G4_DIRTY_CFG;
  if((i==G2_GMU_CMD||i==G2_HUB_CMD)&&m->dirty_late&&m->r->phase==90&&(m->values[G2_CX_POWER]&1))m->values[i]|=G4_DIRTY_CFG;
  if(m->changed_backup&&m->reads==G2_NREGS+1)m->values[i]^=0x1000000;
  if(m->owner&&m->writes==m->owner)m->values[i]^=0x1000000;
  if(m->pending[i]&&!m->update_stuck&&!--m->pending[i]) {
   m->values[i]&=~G2_UPDATE;if(!m->dirty_stuck)m->values[i]&=~G4_DIRTY_CFG;
  }
  if((i==G2_GMU_CMD||i==G2_HUB_CMD)&&(m->values[i]&G2_UPDATE)&&m->cmd_owner_poll) {
   if(++m->cmd_samples==2)m->values[i]^=0x1000000;
  }
  if((i==G2_GMU_CMD||i==G2_HUB_CMD)&&m->transient_clear&&m->writes>=6) {
   if(++m->cmd_samples==2)return m->values[i]&~G4_CMD_PENDING;
   if(m->cmd_samples>=3)m->values[i]|=G4_CMD_PENDING;
  }
  uint32_t v=m->values[i];
  if(i==G2_CX_POWER) {if(m->collapse_preunknown&&m->r->phase==90&&!(v&1)&&!m->cx_settle_at)return (v&~G3_CX_OBSERVED_UPPER)|0x50000000;if(m->collapse_unknown&&m->cx_settle_at)return (v&~G3_CX_OBSERVED_UPPER)|0x50000000;if(m->observed_initial&&!m->writes)return (v&~G3_CX_OBSERVED_UPPER)|m->observed_initial;if(m->bad_power_status&&!(v&1))return (v&~G3_CX_OBSERVED_UPPER)|0x40000000;return (v&1)?v&~G3_CX_OBSERVED_UPPER:v|G3_CX_OBSERVED_UPPER;}
  if(i==G2_AHB)return 0;
  if(g7c_regs[i].ro_mask&&i!=G2_GMU_CMD&&i!=G2_HUB_CMD) {
   if((v&1)&&!(m->values[G2_CX_POWER]&1))v&=~G2_OFF;else v|=G2_OFF;
  }
  if(m->clock_never==(unsigned)i+1&&m->r->phase==3)v=(v&~0x70000000u)|G2_OFF;
  return v;
 }
 if(b==G2_GPUCC&&o==G2_CX_HW_STATUS)return ((m->values[G2_CX_POWER]&1)&&!(m->collapse_never_off&&m->cx_settle_at))||m->power_timeout?0x5ff4:0x80005ff4;
 if(b==G2_GPUCC&&o==G2_GX_POWER)return 0x22f001|(m->gx_active&&(m->r->flags&G2_RATES)?G2_PWR_ON:0);
 if(b==G2_GCC&&o==G2_GCC_MEASURE_STATUS) {
  if(m->stale)return G2_MEASURE_DONE|m->count;
  if(m->measure_running&&!m->never&&m->us>=m->finish)m->status=G2_MEASURE_DONE|m->count;
  return m->status;
 }
 if(b==G2_GMU) {
  if(o==0x14000)return m->active_gmu?0:1;
  if(o==0x14220||o==0x14224) {
   uint64_t ticks=m->us*192/10;
   m->ao_reads++;if(m->bad_counter&&m->ao_reads>=4)ticks+=(1ull<<63);
   return o==0x14220?(uint32_t)ticks:(uint32_t)(ticks>>32);
  }
  return 0;
 }
 if(b==G2_SMMU) {
  m->smmu_reads++;
  if(o==0x20)return m->physical_matches;
  if(o==0x24)return m->bad_layout?0x80000006:0x30000000|m->physical_contexts;
  return 0;
 }
 m->forbidden++;*ok=0;return 0;
}
static void wr(void *arg,uint64_t b,uint32_t o,uint32_t v,int *ok) {
 struct model *m=arg;int i=index_of(b,o);m->writes++;*ok=1;
 if(m->cx_settle_at&&m->us<m->cx_settle_at){m->early_collapse_writes++;*ok=0;return;}
 if(i==G2_CXO_AON||i==G2_DEMET)m->unrequested_stores++;
 if(i<0||!g7c_write_allowed(b,o)){m->forbidden++;*ok=0;return;}
 if(m->corrupt_backup==m->writes)m->r->old[G2_PLL_VOTE]^=0x200;
 uint32_t live=m->values[i];
 if((i==G2_GMU_CMD||i==G2_HUB_CMD)&&m->dirty_store_race)live=m->values[i]&=~G4_DIRTY_CFG;
 if(i==G2_CX_POWER) {
  live=(live&1)?live&~G3_CX_OBSERVED_UPPER:live|G3_CX_OBSERVED_UPPER;
  struct g7c_record forged=*m->r;forged.trace[forged.events-1].value=v^0x08000000;
  if(g7c_store_guard(&forged,b,o,v^0x08000000,live))m->forbidden++;
 }
 if(i==G2_GMU_CMD||i==G2_HUB_CMD) {
  struct g7c_record forged=*m->r;forged.trace[forged.events-1].value=v^G4_DIRTY_CFG;
  if(g7c_store_guard(&forged,b,o,v^G4_DIRTY_CFG,live))m->forbidden++;
  m->dirty_forge_checks++;
 }
 if(!g7c_store_guard(m->r,b,o,v,live)){m->guard_reject++;*ok=0;return;}
 if((v^m->values[i])&~(g7c_regs[i].mask|g7c_regs[i].ro_mask)){m->forbidden++;*ok=0;return;}
 uint32_t was=m->values[i];m->values[i]=(was&~g7c_regs[i].mask)|(v&g7c_regs[i].mask);
 if(i==G2_CX_POWER&&!(v&G2_COLLAPSE)&&(was&G2_COLLAPSE)){m->cx_enable_at=m->us+1u;m->enable_settles++;}
 if(i==G2_CX_POWER&&(v&G2_COLLAPSE)&&!(was&G2_COLLAPSE)) {
  m->cx_settle_at=m->us+101u;m->collapse_settles++;
 }
 if(i==G2_AHB)m->ahb=v&1;
 if((i==G2_GMU_CFG||i==G2_HUB_CFG)&&!m->dirty_missing) {
  unsigned cmd=i==G2_GMU_CFG?G2_GMU_CMD:G2_HUB_CMD;m->values[cmd]|=G4_DIRTY_CFG;
  if(m->dirty_delayed){m->values[cmd]&=~G4_DIRTY_CFG;m->dirty_delay[cmd]=3;}
  m->values[cmd]&=~G2_PWR_ON; /* native G3 CMD:80000000 ->10 after CFG */
 }
 if((i==G2_GMU_CMD||i==G2_HUB_CMD)&&(v&G2_UPDATE)) {
  m->pending[i]=2;
  if(m->clear_immediate){m->pending[i]=0;m->values[i]&=~G4_CMD_PENDING;}
 }
 if(i==G2_MEASURE_CTL) {
  if(v&G2_MEASURE_CLEAR)m->status=0;
  if(!(was&G2_MEASURE_RUN)&&(v&G2_MEASURE_RUN)) {
   uint32_t ticks=v&0xfffff,div=((m->values[G2_GPU_DIV]&15)+1)*((m->values[G2_GCC_DIV]&15)+1),hz=rate(m);
   m->count=hz?(uint32_t)((uint64_t)hz*ticks/(4800000ull*div)):17;
   m->status=0;m->finish=m->us+(uint64_t)ticks*1000000/4800000;m->measure_running=1;
  }
  if(!(v&G2_MEASURE_RUN))m->measure_running=0;
 }
 if(m->fail_write==m->writes)*ok=0;
}
static void delay(void *arg,uint32_t us){struct model*m=arg;if(m->skip_enable_wait&&m->cx_enable_at&&m->us<m->cx_enable_at&&us==1)return;m->us+=us;}
static uint64_t ticks(void *arg){return ((struct model *)arg)->us*192/10;}
static void setup(struct model *m,struct g7c_record *r) {
 memset(m,0,sizeof(*m));memset(r,0,sizeof(*r));m->r=r;
 r->opaque=G7C_PLATFORM_SCOPE;m->physical_matches=9;m->physical_contexts=7;
 const uint32_t initial[G2_NREGS]={
  0x22f001,0x8000,0x80000003,0x80000001,0x300001,
  0x100000,0x80000000,0,0,
  0,0x80000001,0,1,1,0x80000001,0x80002221,0x80000000,
  0x12345600,0x10000000,0x80000000,0x20000,0x4000,0x80000000,0x80000000,0
 };
 memcpy(m->values,initial,sizeof(initial));memcpy(m->initial,initial,sizeof(initial));
}
static int same_rw(struct model *m) {
 for(unsigned i=0;i<G2_NREGS;i++)if((m->values[i]^m->initial[i])&~g7c_regs[i].ro_mask)return 0;
 return m->ahb==0;
}
static int qualified_hook(void*arg,struct g7c_record*r){
 struct model*m=arg;m->hook_calls++;m->hook_writes=m->writes;
 if(r->flags!=127||r->phase!=7||r->restore||r->poison||r->error||(m->values[G2_CX_POWER]&1)){m->forbidden++;return 0;}
 if(m->hook_fail){r->poison=1;return 0;}return 1;
}
int main(void) {
 struct g7c_record r;struct model m;unsigned test=0;setup(&m,&r);struct g7c_io io={&m,rd,wr,delay,ticks,0,0};
 CHECK(g7c_run(&r,&io));CHECK(r.flags==255&&r.error==0&&!r.poison&&r.restore);CHECK(same_rw(&m));CHECK(!m.forbidden&&!m.guard_reject);CHECK(m.smmu_reads>0);
 CHECK(r.measure[0][3]>18600000&&r.measure[2][3]>180000000&&r.measure[3][3]>135000000);CHECK(r.events<192);
 CHECK(r.smmu_contexts==1&&r.smmu_cb_base==0x10000);
 CHECK(r.smmu_matches==6);
 for(unsigned i=6;i<128;i++)CHECK(!r.smr[i][0]&&!r.smr[i][1]);
 for(unsigned i=6;i<128;i++){CHECK(!g7c_read_allowed(&r,G2_SMMU,0x800+4*i));CHECK(!g7c_read_allowed(&r,G2_SMMU,0xc00+4*i));}
 r.smmu_matches=128;r.smmu_contexts=128;
 CHECK(!g7c_read_allowed(&r,G2_SMMU,0x818)&&!g7c_read_allowed(&r,G2_SMMU,0xc18));
 CHECK(!g7c_read_allowed(&r,G2_SMMU,0x11000)&&!g7c_read_allowed(&r,G2_SMMU,0x10000));
 r.smmu_matches=6;r.smmu_contexts=1;
 CHECK(!g7c_read_allowed(&r,G2_SMMU,r.smmu_cb_base+r.smmu_page));
 CHECK(m.dirty_forge_checks==4);CHECK(!((r.final[G2_GMU_CMD]|r.final[G2_HUB_CMD])&G4_CMD_PENDING));
 CHECK(!m.unrequested_stores);CHECK(!g7c_write_allowed(G2_GPUCC,0x9004));CHECK(!g7c_write_allowed(G2_GPUCC,0x900c));
 CHECK(m.collapse_settles==1&&!m.early_collapse_reads&&!m.early_collapse_writes);
 CHECK(m.enable_settles==1&&!m.early_enable_reads);
 CHECK(g7c_read_allowed(&r,G2_GPUCC,0x9004)&&g7c_read_allowed(&r,G2_GPUCC,0x900c));
 unsigned writes=m.writes,reads=m.reads,phase_reads[100];memcpy(phase_reads,m.phase_reads,sizeof(phase_reads));printf("G2 model success bytes=%zu writes=%u reads=%u rates=%u/%u/%u/%u\n",sizeof(r),writes,reads,r.measure[0][3],r.measure[1][3],r.measure[2][3],r.measure[3][3]);
 for(test=1;test<=writes;test++) {
  setup(&m,&r);m.fail_write=test;CHECK(!g7c_run(&r,&io));CHECK(r.poison&&m.writes==test&&!r.restore);CHECK(!m.forbidden);CHECK(r.trace[r.events-1].phase&0x8000);
 }
 for(test=1;test<=80;test++) {
  setup(&m,&r);m.fail_read=test;CHECK(!g7c_run(&r,&io));CHECK(r.poison&&r.error==G2_E_IO);CHECK(!m.forbidden);
 }
 for(test=2;test<100;test++)if(phase_reads[test]) {
  setup(&m,&r);m.fail_read=phase_reads[test];CHECK(!g7c_run(&r,&io));CHECK(r.poison&&r.error==G2_E_IO);CHECK(m.reads==m.fail_read&&!m.forbidden);
 }
 for(test=1;test<=writes;test++) {
  setup(&m,&r);m.owner=test;CHECK(!g7c_run(&r,&io));CHECK(r.poison&&m.writes==test);CHECK(!m.forbidden);
  unsigned observed=0;for(unsigned i=0;i<G2_NREGS;i++)observed|=(r.final[i]^r.old[i])&0x1000000;
  CHECK(observed); /* opaque AHB may defer the visible mismatch to the next register */
 }
 for(test=0;test<10;test++) {
  setup(&m,&r);
  if(test==0)m.values[G2_CX_POWER]|=2;
  if(test==1)m.values[G2_CX_POWER]&=~1u;
  if(test==2)m.values[G2_MEASURE_CTL]|=G2_MEASURE_RUN;
  if(test==3)m.values[G2_GPU_DEBUG_CLK]|=1;
  if(test==4)m.values[G2_GCC_DEBUG_CLK]|=1;
  if(test==5)m.values[G2_GMU_CMD]|=1;
  if(test==6)m.changed_backup=1;
  if(test==7)m.values[G2_GMU_CMD]|=G4_DIRTY_CFG;
  if(test==8)m.values[G2_HUB_CMD]|=G4_DIRTY_CFG;
  if(test==9)m.values[G2_HUB_CMD]|=G2_UPDATE;
  CHECK(!g7c_run(&r,&io));CHECK(!m.writes);
 }
 setup(&m,&r);m.stale=1;test=1001;CHECK(!g7c_run(&r,&io));CHECK(r.error==G2_E_COUNTER&&r.poison&&!r.restore&&!m.smmu_reads);
 setup(&m,&r);m.never=1;test=1002;CHECK(!g7c_run(&r,&io));CHECK(r.poison&&!m.smmu_reads);
 setup(&m,&r);m.off_ahb=1;test=1003;CHECK(!g7c_run(&r,&io));CHECK(r.error==G2_E_RATE&&r.restore&&!r.poison&&same_rw(&m)&&!m.smmu_reads);
 setup(&m,&r);m.bad_layout=1;test=1004;CHECK(!g7c_run(&r,&io));CHECK(r.error==G2_E_LAYOUT&&r.restore&&!r.poison&&same_rw(&m));
 setup(&m,&r);m.active_gmu=1;test=1005;CHECK(!g7c_run(&r,&io));CHECK(r.error==G2_E_ACTIVE&&r.poison&&!m.smmu_reads);
 setup(&m,&r);m.gx_active=1;test=1006;CHECK(!g7c_run(&r,&io));CHECK(r.error==G2_E_ACTIVE&&r.poison&&!m.smmu_reads);
 setup(&m,&r);m.bad_counter=1;test=1007;CHECK(!g7c_run(&r,&io));CHECK(r.error==G2_E_COUNTER&&r.restore&&!r.poison&&same_rw(&m)&&!m.smmu_reads);
 setup(&m,&r);m.skip_enable_wait=1;test=1007;CHECK(!g7c_run(&r,&io));CHECK(r.error==G2_E_OWNER&&r.poison&&m.writes==2&&m.early_enable_reads==1&&!r.restore&&!m.smmu_reads);
 setup(&m,&r);m.power_timeout=1;test=1008;CHECK(!g7c_run(&r,&io));CHECK(r.error==G2_E_POWER&&r.poison&&m.writes==2&&!m.smmu_reads);
 setup(&m,&r);m.update_stuck=1;test=1009;CHECK(!g7c_run(&r,&io));CHECK(r.error==G2_E_UPDATE&&r.poison&&!m.smmu_reads);
 setup(&m,&r);m.corrupt_backup=1;test=1010;CHECK(!g7c_run(&r,&io));CHECK(m.guard_reject==1&&m.writes==1&&r.poison&&!m.smmu_reads);
 setup(&m,&r);m.dirty_missing=1;test=1011;CHECK(!g7c_run(&r,&io));CHECK(r.error==G2_E_UPDATE&&r.poison&&m.writes==5&&!m.smmu_reads);
 setup(&m,&r);m.dirty_stuck=1;test=1012;CHECK(!g7c_run(&r,&io));CHECK(r.error==G2_E_UPDATE&&r.poison&&m.writes==6&&!m.smmu_reads);
 setup(&m,&r);m.clear_immediate=1;test=1013;CHECK(g7c_run(&r,&io));CHECK(r.restore&&!r.poison&&same_rw(&m));
 setup(&m,&r);m.cmd_owner_poll=1;m.update_stuck=1;test=1014;CHECK(!g7c_run(&r,&io));CHECK(r.error==G2_E_OWNER&&r.poison&&m.writes==6&&!m.smmu_reads);
 setup(&m,&r);m.transient_clear=1;m.update_stuck=1;test=1015;CHECK(!g7c_run(&r,&io));CHECK(r.error==G2_E_UPDATE&&r.poison&&m.writes==6&&!m.smmu_reads);
 setup(&m,&r);m.dirty_delayed=1;test=1016;CHECK(g7c_run(&r,&io));CHECK(r.restore&&!r.poison&&same_rw(&m));
 setup(&m,&r);m.dirty_store_race=1;test=1017;CHECK(!g7c_run(&r,&io));CHECK(r.poison&&m.guard_reject==1&&m.writes==6&&!m.smmu_reads);
 setup(&m,&r);m.dirty_late=1;test=1018;CHECK(!g7c_run(&r,&io));CHECK(r.error==G2_E_RESTORE&&r.poison&&!r.restore);
 setup(&m,&r);m.collapse_unknown=1;test=1021;CHECK(!g7c_run(&r,&io));
 CHECK(r.error==G2_E_OWNER&&r.poison&&!r.restore&&r.final[G2_CX_POWER]==0x5022f001);
 CHECK(m.collapse_settles==1&&!m.early_collapse_reads&&!m.early_collapse_writes&&r.trace[r.events-1].index==G2_CX_POWER&&(r.trace[r.events-1].phase&0x8000));
 setup(&m,&r);m.collapse_never_off=1;test=1022;CHECK(!g7c_run(&r,&io));
 CHECK(r.error==G2_E_RESTORE&&r.poison&&!r.restore&&m.collapse_settles==1&&!m.early_collapse_reads&&!m.early_collapse_writes);
 setup(&m,&r);m.collapse_preunknown=1;test=1023;CHECK(!g7c_run(&r,&io));
 CHECK(r.error==G2_E_OWNER&&r.poison&&!r.restore&&!m.collapse_settles&&r.final[G2_CX_POWER]==0x5022f800);
 setup(&m,&r);m.values[G2_GMU_CFG]=m.initial[G2_GMU_CFG]=0x602;m.values[G2_HUB_CFG]=m.initial[G2_HUB_CFG]=0x603;test=1019;
 CHECK(g7c_run(&r,&io));CHECK(r.restore&&!r.poison&&same_rw(&m)&&!m.dirty_forge_checks);
 for(test=0;test<G2_NREGS;test++) {
  if(!((test>=G2_GCC_MEMNOC&&test<=G2_GCC_DDR)||test==G2_CXO||
       (test>=G2_HUB_AON&&test<=G2_SMMU_VOTE)))continue;
  setup(&m,&r);m.clock_never=test+1;CHECK(!g7c_run(&r,&io));
  CHECK(r.error==G2_E_CLOCK&&r.poison&&!r.restore&&!m.smmu_reads&&!m.unrequested_stores);
 }
 for(test=1;test<=128;test++) {
  setup(&m,&r);m.physical_matches=test;m.physical_contexts=7;CHECK(g7c_run(&r,&io));
  CHECK(r.smmu_matches==(test<6?test:6)&&r.smmu_contexts==1);
 }
 setup(&m,&r);test=1024;r.opaque=0;CHECK(!g7c_run(&r,&io));CHECK(r.error==G2_E_LAYOUT&&!m.reads&&!m.writes);
 r.flags=G2_GMU_QUIET;CHECK(!g7c_read_allowed(&r,G2_SMMU,0x20)&&!g7c_read_allowed(&r,G2_SMMU,0x800));
 setup(&m,&r);test=1025;m.physical_matches=0;CHECK(!g7c_run(&r,&io));CHECK(r.error==G2_E_LAYOUT&&r.restore&&!r.poison);
 setup(&m,&r);test=1026;m.physical_matches=129;CHECK(!g7c_run(&r,&io));CHECK(r.error==G2_E_LAYOUT&&r.restore&&!r.poison);
 setup(&m,&r);test=1027;m.physical_contexts=0;CHECK(!g7c_run(&r,&io));CHECK(r.error==G2_E_LAYOUT&&r.restore&&!r.poison);
 setup(&m,&r);test=1028;m.physical_contexts=129;CHECK(!g7c_run(&r,&io));CHECK(r.error==G2_E_LAYOUT&&r.restore&&!r.poison);
 setup(&m,&r);test=1020;
 for(unsigned index=G2_CXO_AON;index<=G2_DEMET;index++) {
  r.flags=G2_BACKUP;r.phase=3;r.events=1;r.trace[0]=(struct g7c_event){(uint16_t)index,0x8003,1};
  uint32_t saved[G2_NREGS];for(unsigned i=0;i<G2_NREGS;i++)saved[i]=r.old[i]&~g7c_regs[i].ro_mask;
  r.backup_crc=g7_clock_crc(saved,sizeof(saved));
  CHECK(!g7c_store_guard(&r,g7c_regs[index].base,g7c_regs[index].offset,1,0));
 }
 for(test=0;test<0x20000;test+=4)CHECK(!g7c_write_allowed(G2_SMMU,test));
 for(test=0;test<0x34000;test+=4)CHECK(!g7c_write_allowed(G2_GMU,test));
 for(test=1;test<32;test++) {setup(&m,&r);m.observed_initial=test<<27;CHECK(!g7c_run(&r,&io));CHECK(!m.writes);}
 setup(&m,&r);m.bad_power_status=1;test=1100;CHECK(!g7c_run(&r,&io));CHECK(r.poison&&r.error==G2_E_OWNER&&m.writes==2&&!m.smmu_reads);CHECK((r.final[G2_CX_POWER]&G3_CX_OBSERVED_UPPER)==0x40000000);
 CHECK(!g7c_run(0,&io));CHECK(!g7c_run(&r,0));
 setup(&m,&r);io.qualified=qualified_hook;test=1200;CHECK(g7c_run(&r,&io));CHECK(m.hook_calls==1&&r.restore&&same_rw(&m)&&!m.forbidden);
 setup(&m,&r);m.hook_fail=1;test=1201;CHECK(!g7c_run(&r,&io));CHECK(m.hook_calls==1&&r.poison&&!r.restore&&m.writes==m.hook_writes&&!m.forbidden);
 printf("G2_ACTUAL_POWER_ENGINE_HOST_PASS checks=%u faultWrites=%u faultReads=80 ownershipWrites=%u no-GMU-SMMU-store\n",checks,writes,writes);return 0;
}
