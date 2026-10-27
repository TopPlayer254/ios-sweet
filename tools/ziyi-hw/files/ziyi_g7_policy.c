/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_g7_policy.h"
static int base(const struct ga_snapshot*s) {
 if(!s)return GA_ARG;
 if(s->facts!=GA_REQUIRED_FACTS||!s->epoch)return GA_FACTS;
 if(s->dt_cb!=6||s->dt_smr!=6||s->dt_handoff_pairs)return GA_DT;
 for(unsigned i=0;i<4;i++)if(s->gr0[0][i]!=s->gr0[1][i])return GA_CHANGED;
 uint32_t id0=s->gr0[0][1],id1=s->gr0[0][2],id2=s->gr0[0][3];
 /* Only the measured non-EXID, stage1/SMS, 4K, no-stage2-only profile. */
 if(!(id0&(1u<<30))||!(id0&(1u<<27))||(id0&(1u<<8))||
    (id0&255)<6||(id1&255)<6||((id1>>16)&255)||
    !(id2&(1u<<12))||(id1&(1u<<31))||((id1>>28)&7)!=3)return GA_ID;
 for(unsigned i=0;i<6;i++)if(s->smr[0][i]!=s->smr[1][i]||s->s2cr[0][i]!=s->s2cr[1][i])return GA_CHANGED;
 return GA_OK;
}
int ga_plan_fresh(const struct ga_snapshot*s,uint32_t busy,struct ga_plan*out) {
 if(!out)return GA_ARG;*out=(struct ga_plan){0};int e=base(s);if(e)return e;
 if(busy&~63u)return GA_ARG;
 /* Exact DT has no handoff list. Conservative fresh instance rejects ALL
  * live routes instead of quietly discarding unknown bootloader owners. */
 for(unsigned i=0;i<6;i++)if(s->smr[0][i]&(1u<<31))return GA_ACTIVE;
 unsigned cb=2;for(;cb<6;cb++)if(!(busy&(1u<<cb)))break;
 if(cb==6)return GA_FULL;
 /* Source allocator reserves CB0/1 for GPU/LPAC; first free stream slot is
  * from the source-clamped six-entry namespace, never physical index6+. */
 out->cb=cb;out->smr=0;out->software_context_map=busy|3u|(1u<<cb);
 out->epoch=s->epoch;out->hardware_write_authority=0;return GA_OK;
}
int ga_ro_offset(const struct ga_snapshot*s,uint32_t off) {
 if(base(s)||off&3)return 0;
 const uint32_t fixed[]={0,0x20,0x24,0x28,0x3c,0x48};
 for(unsigned i=0;i<6;i++)if(off==fixed[i])return 1;
 if((off>=0x800&&off<0x818)||(off>=0xc00&&off<0xc18))return 1;
 /* GR1 page+CBAR/CBA2R only. These fields describe format/type, not security
  * ownership; no interpretation of disabled SCTLR is used here. */
 if((off>=0x1008&&off<0x1018)||(off>=0x1808&&off<0x1818))return 1;
 return 0;
}
static uint64_t align4k(uint64_t x){return (x+4095)&~4095ULL;}
int ga_layout_plan(uint64_t anchor,uint32_t fw,uint32_t backing,struct ga_layout*out) {
 if(!out)return GA_ARG;*out=(struct ga_layout){0};
 if(anchor<GA_ARENA_BASE||anchor>GA_ARENA_END||!fw||fw>0x10000||!backing||backing>0x100000)return GA_RANGE;
 uint64_t at=align4k(anchor);struct ga_layout p={0};p.firmware=at;at+=align4k(fw);
 p.backup0=at;at+=0x8000;p.backup1=at;at+=0x8000;
 p.backing=at;at+=align4k(backing);p.tables=at;p.table_bytes=0x40000;at+=p.table_bytes;p.end=at;
 if(at>GA_ARENA_END)return GA_RANGE;*out=p;return GA_OK;
}
