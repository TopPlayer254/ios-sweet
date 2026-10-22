/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Pure U3 policy. No MMIO or writes; exact U2 hardware profile, fail closed.
 */
#ifndef ZIYI_U3_POLICY_H
#define ZIYI_U3_POLICY_H
#include "u1_core.h"
#define U3_MATCHES 125u
#define U3_REQUEST_MAX (1024u*1024u)
struct u3_smmu {
    uint32_t cr0,cr2,id0,id1,id2,id7;
    uint32_t smr[U3_MATCHES],s2cr[U3_MATCHES];
    uint32_t cbar,cba2r,sctlr,tcr2,ttbr0lo,ttbr0hi,ttbr1lo,ttbr1hi,tcr;
};
static inline int u3_identity(const struct u3_smmu *s) {
    /* Ordinary SMR matching only. No CLIENTPD, EXIDENABLE, EXSMRGENABLE,
     * PTM/FB override or unmeasured global attribute configuration. */
    if((s->cr0!=0x280406u && s->cr0!=0x290406u)||s->cr2||
       s->id0!=0x4c017e7du||s->id1!=0x6000004eu||s->id2!=0x5111u||s->id7!=0x24u) return 0;
    unsigned matches=0;
    for(unsigned i=0;i<U3_MATCHES;i++) {
        uint32_t m=s->smr[i];
        if(!(m&0x80000000u)) continue;
        if(((0x20u^(m&0x7fffu))&~((m>>16)&0x7fffu))!=0) continue;
        /* Only the actually surveyed exact SID, index and context. */
        if(i!=0||m!=0x80000020u||s->s2cr[i]!=0) return 0;
        matches++;
    }
    return matches==1 && s->cbar==0x1f000u && s->cba2r==1 &&
        s->sctlr==0x0a9f00e0u && s->tcr2==0x60u &&
        !s->ttbr0lo&&!s->ttbr0hi&&!s->ttbr1lo&&!s->ttbr1hi&&!s->tcr;
}
static inline int u3_dma_range(const struct u3_smmu *s,uint64_t pa,uint64_t bytes,uint32_t cap) {
    static const unsigned bits[]={32,36,40,42,44,48};
    unsigned ias=s->id2&15u,oas=(s->id2>>4)&15u;
    if(!bytes||pa>UINT64_MAX-(bytes-1)||ias>=6||oas>=6) return 0;
    uint64_t last=pa+bytes-1;
    return !(last>>bits[ias]) && !(last>>bits[oas]) &&
        (!(last>>32)||(cap&(1u<<24)));
}
static inline int u3_firmware_span(uint64_t pa,uint64_t n) {
    /* Runtime M0 DT: separate lower firmware RAM, outside XNU's arena. */
    return n && pa>=0xf1400000ull && pa<0x100000000ull && n<=0x100000000ull-pa;
}
static inline int u4_inert_slot(const uint8_t *d) {
    for(unsigned i=0;i<32;i++) if(i!=8&&d[i]) return 0;
    return d[8]==0||d[8]==15;
}
static inline int u3_old_utrd(const uint8_t *d,uint64_t *ucd,unsigned *prdt,unsigned *n) {
    uint32_t h=u1_le32(d),ocs=u1_le32(d+8),w6=u1_le32(d+24),w7=u1_le32(d+28);
    unsigned dir=(h>>25)&3u;
    uint64_t pa=u1_le64(d+16);
    unsigned roff=w6>>16,rlen=w6&65535u,poff=w7>>16,pnum=w7&65535u;
    if(((h>>28)&15u)>1 || (dir!=0&&dir!=2) || (ocs&255u)!=0 ||
       (pa&127u)||!u3_firmware_span(pa,4096)||
       roff<8||rlen<8||roff>1024||rlen>1024-roff||
       pnum>32||poff>1024||(pnum&&(poff<8||pnum*4>1024-poff))||
       (pnum&&poff<roff+rlen)) return 0;
    *ucd=pa;*prdt=poff*4;*n=pnum;return 1;
}
static inline int u3_old_ucd(const uint8_t *u,unsigned poff,unsigned n) {
    if(u[0]==0) {
        /* Completed NOP OUT: no command or data payload. Firmware may retain
         * the legacy SCSI CT encoding; this queue is read, never replayed. */
        if(n||u[1]||u[2]) return 0;
        for(unsigned i=4;i<32;i++) if(u[i]) return 0;
        return 1;
    }
    if(u[0]!=1 || (u[1]&0x20u) || (u[1]&0xc0u)==0xc0u || u[2]>7 ||
       poff>4096 || n>32 || n*16>4096-poff) return 0;
    unsigned op=u[16];
    if(op!=0 && op!=0x12 && op!=3 && op!=0x25 && op!=0x28 &&
       op!=0x88 && op!=0xa0 && !(op==0x9e&&u[17]==0x10)) return 0;
    uint64_t capacity=0;
    for(unsigned i=0;i<n;i++) {
        const uint8_t *p=u+poff+i*16;
        uint32_t dbc=u1_le32(p+12);
        uint64_t pa=u1_le64(p);
        if((pa&3u)||u1_le32(p+8)||(dbc&~0x3ffffu)||!u3_firmware_span(pa,(uint64_t)dbc+1)) return 0;
        capacity+=(uint64_t)dbc+1;
    }
    uint32_t expected=u1_be32(u+12);
    if(expected>capacity || (expected && !(u[1]&0x40u)) || (!expected&&n)) return 0;
    return 1;
}
static inline int u3_partition_io(uint64_t first,uint64_t last,uint32_t physical,
                                  uint32_t logical,int64_t block,uint32_t count,
                                  uint64_t *off,uint64_t *bytes) {
    if((physical!=512&&physical!=4096)||logical<512||logical>physical||
       (logical&(logical-1))||first>last||last==UINT64_MAX||
       last-first+1>UINT64_MAX/physical||block<0||count>U3_REQUEST_MAX||
       (uint64_t)block>UINT64_MAX/logical) return 0;
    uint64_t size=(last-first+1)*physical,pos=(uint64_t)block*logical;
    if(pos>size||count>size-pos) return 0;
    *off=pos;*bytes=size;return 1;
}
#endif
