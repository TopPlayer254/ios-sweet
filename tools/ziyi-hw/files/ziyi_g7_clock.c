/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_g7_clock.h"
static int fail(struct g7c_record *r,unsigned error,unsigned poison) {
 if(!r->error)r->error=error;r->poison|=poison;return 0;
}
static void phase(struct g7c_record *r,const struct g7c_io *io,unsigned value) {
 r->phase=value;if(io->phase)io->phase(io->ctx,value);
}
int g7c_write_allowed(uint64_t b,uint32_t o) {
 /* The exact GMU consumer does not request CXO_AON or DEMET, and none of
  * the requested branches has either as a parent. Keep their table masks
  * unchanged for comparison, but grant no store authority for these rows. */
 for(unsigned i=0;i<G2_NREGS;i++)if(g7c_regs[i].base==b&&g7c_regs[i].offset==o)
  return i!=G2_CXO_AON&&i!=G2_DEMET;
 return 0;
}
static int cx_status_valid(uint32_t value) {
 uint32_t status=value&G3_CX_OBSERVED_UPPER;return status==0||status==G3_CX_OBSERVED_UPPER;
}
int g7c_store_guard(const struct g7c_record *r,uint64_t b,uint32_t o,uint32_t value,uint32_t now) {
 if(!r||r->poison||!(r->flags&G2_BACKUP)||!r->events||r->events>192)return 0;
 if(!g7c_write_allowed(b,o))return 0;
 if(r->phase!=2&&r->phase!=3&&r->phase!=4&&r->phase!=90)return 0;
 unsigned index=G2_NREGS;
 for(unsigned i=0;i<G2_NREGS;i++)if(g7c_regs[i].base==b&&g7c_regs[i].offset==o)index=i;
 if(index==G2_NREGS)return 0;
 if(index==G2_CX_POWER&&!cx_status_valid(now))return 0;
 /* The extra upper bits are observed volatile, NOT declared read-only.
  * Never allow a store to change them, even with a forged pending trace. */
 if(index==G2_CX_POWER&&((value^now)&G3_CX_OBSERVED_UPPER))return 0;
 /* CMD_DIRTY_CFG is status, never writable authority. Preserve the actual
  * fresh bit even if an attacker forges the pending trace. UPDATE is the
  * only writable bit and must not inherit a still-running update. */
 if(index==G2_GMU_CMD||index==G2_HUB_CMD) {
  if(((value^now)&G4_DIRTY_CFG)||(now&G2_UPDATE)||!(now&G4_DIRTY_CFG)||!(value&G2_UPDATE))return 0;
 }
 const struct g7c_reg *d=&g7c_regs[index];const struct g7c_event *e=&r->trace[r->events-1];
 if(e->index!=index||e->phase!=(r->phase|0x8000u)||e->value!=value)return 0;
 uint32_t saved[G2_NREGS];for(unsigned i=0;i<G2_NREGS;i++)saved[i]=r->old[i]&~g7c_regs[i].ro_mask;
 if(g7_clock_crc(saved,sizeof(saved))!=r->backup_crc)return 0;
 if(((value^now)&~(d->mask|d->ro_mask))||((now^r->old[index])&~(d->mask|d->ro_mask)))return 0;
 return ((now^r->expected[index])&(d->mask&~d->opaque_mask))==0;
}
int g7c_read_allowed(const struct g7c_record *r,uint64_t b,uint32_t o) {
 if(o&3)return 0;
 for(unsigned i=0;i<G2_NREGS;i++)if(g7c_regs[i].base==b&&g7c_regs[i].offset==o)return 1;
 if(b==G2_GPUCC&&(o==G2_CX_HW_STATUS||o==G2_GX_POWER))return 1;
 if(b==G2_GCC&&o==G2_GCC_MEASURE_STATUS)return 1;
 if((r->flags&G2_RATES)&&b==G2_GMU) {
  for(unsigned i=0;i<8;i++)if(o==g7c_gmu_offsets[i])return 1;
  return o==0x14220||o==0x14224;
 }
 if((r->flags&G2_GMU_QUIET)&&(r->opaque&G7C_PLATFORM_SCOPE)&&b==G2_SMMU) {
  const uint32_t globals[]={0,0x20,0x24,0x28,0x3c,0x48};
  for(unsigned i=0;i<6;i++)if(o==globals[i])return 1;
  unsigned limit=r->smmu_matches<G6_HLOS_SMRS?r->smmu_matches:G6_HLOS_SMRS;
  if((o>=0x800&&o<0x800+4*limit)||(o>=0xc00&&o<0xc00+4*limit))return 1;
  if(r->smmu_contexts==1&&r->smmu_cb_base==0x10000&&(r->smmu_page==0x1000||r->smmu_page==0x10000)&&o>=r->smmu_cb_base&&o<0x20000) {
   unsigned bank=(o-r->smmu_cb_base)/r->smmu_page,within=(o-r->smmu_cb_base)%r->smmu_page;
   const uint32_t words[]={0,0x20,0x24,0x30,0x58,0x60,0x64,0x68};
   for(unsigned i=0;i<8;i++)if(bank==0&&within==words[i])return 1;
  }
 }
 return 0;
}
static uint32_t read_word(struct g7c_record *r,const struct g7c_io *io,uint64_t b,uint32_t o) {
 if(r->poison)return 0;
 if(!g7c_read_allowed(r,b,o)){fail(r,G2_E_OWNER,1);return 0;}
 int ok=0;uint32_t v=io->read(io->ctx,b,o,&ok);r->base.reads++;
 if(!ok)fail(r,G2_E_IO,1);return v;
}
static uint32_t reg(struct g7c_record *r,const struct g7c_io *io,unsigned i) {
 uint32_t value=read_word(r,io,g7c_regs[i].base,g7c_regs[i].offset);
 /* Keep the actual latest qualified read even when a later ownership check fails. */
 if(!r->poison) {r->final[i]=value;if(i==G2_CX_POWER&&!cx_status_valid(value))fail(r,G2_E_OWNER,1);}
 return value;
}
static int wait_mask(struct g7c_record *,const struct g7c_io *,uint64_t,uint32_t,uint32_t,uint32_t,unsigned);
/* Every store is a preserved masked update to a fixed control register.
 * RO status bits and the documented AHB opaque bit are treated explicitly. */
static int put(struct g7c_record *r,const struct g7c_io *io,unsigned i,uint32_t bits,int pulse) {
 if(r->poison||i>=G2_NREGS)return 0;
 const struct g7c_reg *d=&g7c_regs[i];bits&=d->mask;
 uint32_t now=reg(r,io,i),ackmask=d->mask&~d->opaque_mask;
 if(r->poison)return 0;
 if(((now^r->old[i])&~(d->mask|d->ro_mask))||((now^r->expected[i])&ackmask))return fail(r,G2_E_OWNER,1);
 if((r->expected[i]&d->mask)==bits)return 1;
 if(!g7c_write_allowed(d->base,d->offset))return fail(r,G2_E_OWNER,1);
 if(r->events>=192)return fail(r,G2_E_EVENTS,1);
 uint32_t value=(now&~d->mask)|bits;int ok=0;
 r->trace[r->events++]=(struct g7c_event){(uint16_t)i,(uint16_t)(r->phase|0x8000u),value};
 io->write(io->ctx,d->base,d->offset,value,&ok);
 if(!ok)return fail(r,G2_E_IO,1);
 /* No MMIO qualification during the source-defined collapse transition.
  * The pre-store native guard still preserves the actual observed upper
  * bits. Do not add transient states to its authority/envelope. Only the
  * first stable read after 1us+100us can qualify this store. */
 if(i==G2_CX_POWER&&(bits&G2_COLLAPSE))io->delay(io->ctx,G6_COLLAPSE_SETTLE_US);
 else if(i==G2_CX_POWER&&((now^value)&G2_COLLAPSE)) {
  /* Vendor enable: barrier, >=8XO cycles, actual HW_CTRL PWR_ON, then
   * qualify the stable GDSCR. Do not interpret its transition as ownership. */
  io->delay(io->ctx,1);
  if(!wait_mask(r,io,G2_GPUCC,G2_CX_HW_STATUS,G2_PWR_ON,G2_PWR_ON,50000))return fail(r,G2_E_POWER,1);
 }
 uint32_t after=reg(r,io,i);if(r->poison)return 0;
 if(pulse)ackmask&=~G2_UPDATE;
 if(((after^now)&~(d->mask|d->ro_mask))||((after^value)&ackmask))return fail(r,G2_E_OWNER,1);
 r->expected[i]=(r->expected[i]&~d->mask)|bits;
 r->trace[r->events-1].phase=(uint16_t)r->phase;
 if(d->opaque_mask)r->opaque|=1u<<i;
 return 1;
}
static int wait_mask(struct g7c_record *r,const struct g7c_io *io,uint64_t b,uint32_t o,uint32_t mask,uint32_t wanted,unsigned us) {
 for(unsigned i=0;i<us;i++) {
  uint32_t v=read_word(r,io,b,o);if(r->poison)return 0;
  if((v&mask)==wanted)return 1;io->delay(io->ctx,1);
 }
 return 0;
}
static int branch_ready(struct g7c_record *r,const struct g7c_io *io,unsigned index) {
 /* Exact vendor clk_branch2_check_halt: an enabling branch has CLK_OFF=0
  * or NOC_FSM_STATUS=ON. AHB is explicitly BRANCH_HALT_DELAY; its clock
  * remains unqualified until the independent measured-rate gate passes. */
 if(index==G2_AHB){io->delay(io->ctx,10);return 1;}
 const struct g7c_reg *d=&g7c_regs[index];
 for(unsigned i=0;i<200;i++) {
  uint32_t value=reg(r,io,index);if(r->poison)return 0;
  if(((value^r->old[index])&~(d->mask|d->ro_mask))||
     ((value^r->expected[index])&(d->mask&~d->opaque_mask)))return fail(r,G2_E_OWNER,1);
  if(!(value&G2_OFF)||(value&0xf0000000u)==0x20000000u)return 1;
  io->delay(io->ctx,1);
 }
 return fail(r,G2_E_CLOCK,1);
}
static int update(struct g7c_record *r,const struct g7c_io *io,unsigned cfg,unsigned cmd,uint32_t bits) {
 /* No inherited shadow transaction. Observe before changing CFG, including
  * restoration. bit31 may change autonomously; all other unknown bits must
  * still match the saved control. No blind observation-mask broadening. */
 uint32_t before=reg(r,io,cmd);if(r->poison)return 0;
 if((before^r->old[cmd])&~(G2_UPDATE|G4_CMD_STATUS))return fail(r,G2_E_OWNER,1);
 if(before&G4_CMD_PENDING)return fail(r,G2_E_UPDATE,1);
 int changed=((r->expected[cfg]^bits)&g7c_regs[cfg].mask)!=0;
 if(!changed)return 1;
 if(!put(r,io,cfg,bits,0))return 0;
 /* A fresh CFG write must produce DIRTY before the UPDATE store. Hardware
  * can clear UPDATE synchronously, so completion uses a fresh DIRTY1 ->
  * post-store stable DIRTY0/UPDATE0 transition rather than requiring a
  * sampled UPDATE1. The issued UPDATE1 remains in the qualified trace. */
 unsigned observed=0;
 for(unsigned i=0;i<500;i++) {
  uint32_t v=reg(r,io,cmd);if(r->poison)return 0;
  if((v^r->old[cmd])&~(G2_UPDATE|G4_CMD_STATUS))return fail(r,G2_E_OWNER,1);
  if(v&G2_UPDATE)return fail(r,G2_E_OWNER,1);
  if(v&G4_DIRTY_CFG){observed=1;break;}io->delay(io->ctx,1);
 }
 if(!observed)return fail(r,G2_E_UPDATE,1);
 if(!put(r,io,cmd,G2_UPDATE,1))return 0;
 unsigned stable=0;
 for(unsigned i=0;i<500;i++) {
  uint32_t v=reg(r,io,cmd);if(r->poison)return 0;
  if((v^r->old[cmd])&~(G2_UPDATE|G4_CMD_STATUS))return fail(r,G2_E_OWNER,1);
  if(!(v&G4_CMD_PENDING)){if(++stable==2){r->expected[cmd]&=~G2_UPDATE;return 1;}}
  else stable=0;
  io->delay(io->ctx,1);
 }
 return fail(r,G2_E_UPDATE,1);
}
static int restore_debug(struct g7c_record *r,const struct g7c_io *io) {
 for(unsigned i=G2_NREGS;i-->G2_GPU_MUX;)if(!put(r,io,i,r->old[i],0))return 0;
 return 1;
}
static int count(struct g7c_record *r,const struct g7c_io *io,uint32_t ticks,uint32_t *result) {
 uint32_t base=r->expected[G2_MEASURE_CTL]&~G2_MEASURE_MASK;
 if(!put(r,io,G2_MEASURE_CTL,base|G2_MEASURE_CLEAR,0))return 0;
 io->delay(io->ctx,2);
 if(!put(r,io,G2_MEASURE_CTL,base|ticks,0)||!put(r,io,G2_MEASURE_CTL,base|ticks|G2_MEASURE_RUN,0))return 0;
 unsigned busy=0;uint32_t status=0;
 for(unsigned i=0;i<200000;i++) {
  status=read_word(r,io,G2_GCC,G2_GCC_MEASURE_STATUS);if(r->poison)return 0;
  if(!(status&G2_MEASURE_DONE))busy=1;
  if(busy&&(status&G2_MEASURE_DONE))break;
  if(i==199999)return fail(r,G2_E_COUNTER,1);
  io->delay(io->ctx,1);
 }
 if(!put(r,io,G2_MEASURE_CTL,base|ticks,0))return 0;
 *result=status&G2_COUNTER_MASK;return 1;
}
static int measure(struct g7c_record *r,const struct g7c_io *io,unsigned which) {
 uint32_t select=g7c_clock_selects[which],brief=0,full=0;r->measure[which][0]=select;
 if(!put(r,io,G2_GPU_MUX,select,0)||!put(r,io,G2_GPU_DIV,1,0)||!put(r,io,G2_GPU_DEBUG_CLK,1,0)||
    !put(r,io,G2_GCC_MUX,G2_GCC_GPU_SELECT,0)||!put(r,io,G2_GCC_DIV,1,0)||!put(r,io,G2_GCC_DEBUG_CLK,1,0)||!put(r,io,G2_XO4,1,0))return 0;
 if(!count(r,io,G2_BRIEF,&brief)||!count(r,io,G2_FULL,&full))return 0;
 r->measure[which][1]=brief;r->measure[which][2]=full;
 uint64_t hz=brief==full?0:(((uint64_t)full*10+15)*4800000/((uint64_t)G2_FULL*10+35))*4;
 if(hz>1000000000)return fail(r,G2_E_RATE,0);
 r->measure[which][3]=(uint32_t)hz;
 return restore_debug(r,io);
}
static uint64_t ao_counter(struct g7c_record *r,const struct g7c_io *io) {
 for(unsigned i=0;i<3;i++) {
  uint32_t hi=read_word(r,io,G2_GMU,0x14224),lo=read_word(r,io,G2_GMU,0x14220),again=read_word(r,io,G2_GMU,0x14224);
  if(hi==again)return ((uint64_t)hi<<32)|lo;
 }
 fail(r,G2_E_COUNTER,0);return 0;
}
static int smmu_snapshot(struct g7c_record *r,const struct g7c_io *io) {
 const uint32_t global[]={0,0x20,0x24,0x28,0x3c,0x48},words[]={0,0x20,0x24,0x30,0x58,0x60,0x64,0x68};
 for(unsigned i=0;i<6;i++)r->smmu[i]=read_word(r,io,G2_SMMU,global[i]);
 if(r->poison)return 0;
 uint32_t idr0=r->smmu[1],idr1=r->smmu[2];
 unsigned page=(idr1>>31)?0x10000:0x1000,pages=2u<<((idr1>>28)&7),banks=idr1&255,matches=idr0&255;
 if(!banks||banks>128||!matches||matches>128||2u*pages*page!=0x20000)return fail(r,G2_E_LAYOUT,0);
 if(banks>G6_HLOS_CONTEXTS)banks=G6_HLOS_CONTEXTS;
 if(matches>G6_HLOS_SMRS)matches=G6_HLOS_SMRS;
 /* Only CB0 is qualified for this first survey. Other banks include secure
  * contexts; a stream-table observation does not authorize reading them. */
 banks=1;
 if(banks*page>0x20000-pages*page)return fail(r,G2_E_LAYOUT,0);
 r->smmu_page=page;r->smmu_cb_base=pages*page;r->smmu_contexts=banks;r->smmu_matches=matches;
 for(unsigned i=0;i<matches;i++) {r->smr[i][0]=read_word(r,io,G2_SMMU,0x800+4*i);r->smr[i][1]=read_word(r,io,G2_SMMU,0xc00+4*i);}
 for(unsigned b=0;b<banks;b++)for(unsigned i=0;i<8;i++)r->cb[b][i]=read_word(r,io,G2_SMMU,r->smmu_cb_base+b*page+words[i]);
 if(r->poison)return 0;r->flags|=G2_SMMU_CAPTURED;return 1;
}
static int cleanup(struct g7c_record *r,const struct g7c_io *io) {
 phase(r,io,90);if(r->poison)return 0;
 if(!restore_debug(r,io))return 0;
 for(unsigned i=G2_SMMU_VOTE+1;i-->G2_AHB;) {
  uint32_t value=r->old[i];if(i==G2_CX_GMU)value=(value&1)|0xf0;
  if(!put(r,io,i,value,0))return 0;
 }
 if(!update(r,io,G2_HUB_CFG,G2_HUB_CMD,r->old[G2_HUB_CFG])||!update(r,io,G2_GMU_CFG,G2_GMU_CMD,r->old[G2_GMU_CFG]))return 0;
 for(unsigned i=G2_GCC_DDR+1;i-->G2_PLL_VOTE;)if(!put(r,io,i,r->old[i],0))return 0;
 if(!put(r,io,G2_CX_POWER,r->old[G2_CX_POWER],0))return 0;
 /* put() already completed the source1us+100us settle before its postread. */
 r->cx_off_status=read_word(r,io,G2_GPUCC,G2_CX_HW_STATUS);
 if(r->poison)return 0;
 /* Do not remove the stagger workaround while the domain might still be on. */
 if(r->cx_off_status&G2_PWR_ON)return fail(r,G2_E_RESTORE,1);
 if(!put(r,io,G2_CX_GMU,r->old[G2_CX_GMU],0))return 0;
 for(unsigned i=0;i<G2_NREGS;i++) {
  r->final[i]=reg(r,io,i);if(r->poison)return 0;
  if((r->final[i]^r->old[i])&~g7c_regs[i].ro_mask)return fail(r,G2_E_RESTORE,1);
 }
 if(r->final[G2_CX_POWER]&G3_CX_OBSERVED_UPPER)return fail(r,G2_E_RESTORE,1);
 if((r->final[G2_GMU_CMD]|r->final[G2_HUB_CMD])&G4_CMD_PENDING)return fail(r,G2_E_RESTORE,1);
 r->gx_after=read_word(r,io,G2_GPUCC,G2_GX_POWER);if(r->poison)return 0;
 if((r->gx_after^r->gx_before)&~G2_PWR_ON)return fail(r,G2_E_RESTORE,1);
 if(r->gx_after&G2_PWR_ON)return fail(r,G2_E_ACTIVE,1);
 r->restore=1;r->flags|=G2_RESTORED;return 1;
}
int g7c_run(struct g7c_record *r,const struct g7c_io *io) {
 if(!r||!io||!io->read||!io->write||!io->delay||!io->ticks)return 0;
 phase(r,io,1);
 if(!(r->opaque&G7C_PLATFORM_SCOPE))return fail(r,G2_E_LAYOUT,0);
 for(unsigned i=0;i<G2_NREGS;i++)r->old[i]=r->expected[i]=reg(r,io,i);
 if(r->poison)return 0;
 uint32_t masked_old[G2_NREGS],masked_live[G2_NREGS];
 for(unsigned i=0;i<G2_NREGS;i++) {
  r->final[i]=reg(r,io,i);masked_old[i]=r->old[i]&~g7c_regs[i].ro_mask;masked_live[i]=r->final[i]&~g7c_regs[i].ro_mask;
  if(masked_live[i]!=masked_old[i])return fail(r,G2_E_BACKUP,0);
 }
 if(r->poison)return 0;
 r->gx_before=read_word(r,io,G2_GPUCC,G2_GX_POWER);r->cx_on_status=read_word(r,io,G2_GPUCC,G2_CX_HW_STATUS);
 if(r->poison)return 0;
 if((r->old[G2_CX_POWER]&(G2_PWR_ON|G2_HW_CONTROL|G2_COLLAPSE))!=G2_COLLAPSE||
    (r->gx_before&(G2_PWR_ON|G2_HW_CONTROL|G2_COLLAPSE))!=G2_COLLAPSE||(r->cx_on_status&G2_PWR_ON)||
    ((r->old[G2_GMU_CMD]|r->old[G2_HUB_CMD])&G4_CMD_PENDING)||
    (r->old[G2_MEASURE_CTL]&(G2_MEASURE_RUN|G2_MEASURE_CLEAR))||
    ((r->old[G2_GPU_DEBUG_CLK]|r->old[G2_GCC_DEBUG_CLK])&1))return fail(r,G2_E_INITIAL,0);
 r->backup_crc=g7_clock_crc(masked_old,sizeof(masked_old));
 if(r->backup_crc!=g7_clock_crc(masked_live,sizeof(masked_live)))return fail(r,G2_E_BACKUP,0);
 r->flags|=G2_BACKUP;phase(r,io,2);
 if(!put(r,io,G2_CX_GMU,(r->old[G2_CX_GMU]&1)|0xf0,0)||!put(r,io,G2_CX_POWER,r->old[G2_CX_POWER]&~G2_COLLAPSE,0))return 0;
 /* The mandatory enable delay occurs inside put, before its first readback. */
 if(!wait_mask(r,io,G2_GPUCC,G2_CX_HW_STATUS,G2_PWR_ON,G2_PWR_ON,50000))return fail(r,G2_E_POWER,1);
 r->cx_on_status=read_word(r,io,G2_GPUCC,G2_CX_HW_STATUS);r->flags|=G2_CX_ON;
 if(!put(r,io,G2_CX_POWER,(r->old[G2_CX_POWER]&~G2_COLLAPSE)|G2_RETAIN,0))return 0;
 io->delay(io->ctx,2);phase(r,io,3);
 if(!put(r,io,G2_PLL_VOTE,r->old[G2_PLL_VOTE]|0x10000,0)||!update(r,io,G2_GMU_CFG,G2_GMU_CMD,0x602)||!update(r,io,G2_HUB_CFG,G2_HUB_CMD,0x603))return 0;
 for(unsigned i=G2_GCC_MEMNOC;i<=G2_GCC_DDR;i++)
  if(!put(r,io,i,1,0)||!branch_ready(r,io,i))return 0;
 for(unsigned i=G2_AHB;i<=G2_SMMU_VOTE;i++) {
  if(i==G2_CXO_AON||i==G2_DEMET)continue;
  uint32_t value=i==G2_CX_GMU?0xf1:1;
  if(!put(r,io,i,value,0)||!branch_ready(r,io,i))return 0;
 }
 r->flags|=G2_CLOCKS;phase(r,io,4);
 for(unsigned i=0;i<4;i++) {
  if(!measure(r,io,i))goto done;
  uint32_t hz=r->measure[i][3];
  if((i==0&&(hz<18600000||hz>19800000))||(i==1&&(hz<1000000||hz>300000000))||
     (i==2&&(hz<180000000||hz>220000000))||(i==3&&(hz<135000000||hz>165000000))) {fail(r,G2_E_RATE,0);goto done;}
 }
 r->flags|=G2_RATES;phase(r,io,5);
 for(unsigned i=0;i<8;i++)r->gmuregs[i]=read_word(r,io,G2_GMU,g7c_gmu_offsets[i]);
 if(r->poison)return 0;
 if(!(r->gmuregs[0]&1)||(r->gmuregs[4]&1)||r->gmuregs[2]==0xbabeface)return fail(r,G2_E_ACTIVE,1);
 r->gx_after=read_word(r,io,G2_GPUCC,G2_GX_POWER);if(r->gx_after&G2_PWR_ON)return fail(r,G2_E_ACTIVE,1);
 r->flags|=G2_GMU_QUIET;phase(r,io,6);r->arm_counter[0]=io->ticks(io->ctx);r->gmu_counter[0]=ao_counter(r,io);
 io->delay(io->ctx,1000);r->gmu_counter[1]=ao_counter(r,io);r->arm_counter[1]=io->ticks(io->ctx);
 if(r->poison)return 0;
 uint64_t arm=r->arm_counter[1]-r->arm_counter[0],gpu=r->gmu_counter[1]-r->gmu_counter[0];
 if(!arm||arm>19200000||gpu>19200000||gpu*100<arm*90||gpu*100>arm*110){fail(r,G2_E_COUNTER,0);goto done;}
 r->flags|=G2_COUNTER;phase(r,io,7);smmu_snapshot(r,io);
 if(!r->error&&!r->poison&&io->qualified&&!io->qualified(io->ctx,r))fail(r,G2_E_ACTIVE,0);
done:
 if(!r->poison)cleanup(r,io);
 return r->error==0&&r->restore&&(r->flags&255)==255;
}
