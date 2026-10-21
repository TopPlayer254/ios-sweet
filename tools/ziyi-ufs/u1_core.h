/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Bounded, allocation-free read-only UFS probe policy. No hardware access here.
 */
#ifndef ZIYI_U1_CORE_H
#define ZIYI_U1_CORE_H
#include <stdint.h>
#include <stddef.h>
#define U1_TABLE_MAX 16384u
#define U1_BLOCK_MAX 4096u
#define U1_DMA_BYTES 0x8000u
#define U1_DATA_OFF 0x4000u
struct u1_cmd { uint8_t cdb[16]; uint8_t lun; uint32_t bytes; };
struct u1_gpt {
    uint64_t current,other,first,last,entries_lba;
    uint32_t count,table_crc,header_crc;
    uint8_t guid[16];
};
struct u1_partition { uint64_t first,last; uint8_t guid[16]; };
static inline uint32_t u1_le32(const uint8_t *p) {
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static inline uint64_t u1_le64(const uint8_t *p) { return u1_le32(p)|(uint64_t)u1_le32(p+4)<<32; }
static inline uint32_t u1_be32(const uint8_t *p) {
    return (uint32_t)p[3]|(uint32_t)p[2]<<8|(uint32_t)p[1]<<16|(uint32_t)p[0]<<24;
}
static inline uint64_t u1_be64(const uint8_t *p) { return (uint64_t)u1_be32(p)<<32|u1_be32(p+4); }
static inline void u1_putbe32(uint8_t *p,uint32_t x) {
    p[0]=(uint8_t)(x>>24);p[1]=(uint8_t)(x>>16);p[2]=(uint8_t)(x>>8);p[3]=(uint8_t)x;
}
static inline int u1_equal(const void *aa,const void *bb,size_t n) {
    const uint8_t *a=aa,*b=bb; for(size_t i=0;i<n;i++) if(a[i]!=b[i]) return 0; return 1;
}
static inline int u1_nonzero(const uint8_t *p,size_t n) {
    for(size_t i=0;i<n;i++) if(p[i]) return 1; return 0;
}
static inline uint32_t u1_crc(const uint8_t *p,size_t n,size_t zero_start,size_t zero_end) {
    uint32_t c=~0u;
    for(size_t i=0;i<n;i++) {
        c^=(i>=zero_start&&i<zero_end)?0:p[i];
        for(unsigned j=0;j<8;j++) c=(c>>1)^(0xedb88320u & (0u-(c&1)));
    }
    return ~c;
}
/* Only these four generated CDBs can enter the hardware backend. */
static inline int u1_command(struct u1_cmd *out,unsigned op,unsigned lun,
                             uint64_t lba,uint32_t count,uint32_t bs,uint64_t blocks) {
    struct u1_cmd c={{0},0,0};
    if(lun>7) return 0;
    c.lun=(uint8_t)lun;c.cdb[0]=(uint8_t)op;
    if(op==0 && !lba && !count) { }
    else if(op==0x25 && !lba && !count) c.bytes=8;
    else if(op==0x9e && !lba && !count) { c.cdb[1]=0x10;c.cdb[13]=32;c.bytes=32; }
    else if(op==0x28) {
        if((bs!=512&&bs!=4096)||!count||count>U1_BLOCK_MAX/bs||!blocks||
           lba>=blocks||count>blocks-lba||lba>UINT32_MAX||
           (uint64_t)count-1>UINT32_MAX-lba) return 0;
        u1_putbe32(c.cdb+2,(uint32_t)lba);
        c.cdb[7]=(uint8_t)(count>>8);c.cdb[8]=(uint8_t)count;c.bytes=count*bs;
    } else return 0;
    *out=c;return 1;
}
static inline int u1_capacity(const uint8_t *p,size_t n,uint64_t *blocks,uint32_t *bs) {
    if(n!=8&&n!=32) return 0;
    uint64_t last=n==8?u1_be32(p):u1_be64(p);
    uint32_t size=u1_be32(p+(n==8?4:8));
    /* READ(10) implementation deliberately rejects larger media. */
    if(last==UINT64_MAX||last>=UINT32_MAX||last<8||(size!=512&&size!=4096)) return 0;
    *blocks=last+1;*bs=size;return 1;
}
/* Conservative SMMUv2 proof: one explicit bypass match, no global override,
 * no extended SID mode. Never infer bypass from an unmatched stream. */
static inline int u1_smmu_bypass(uint32_t cr0,uint32_t id0,const uint32_t *smr,
                                const uint32_t *s2cr,unsigned n,unsigned sid) {
    if(cr0&((1u<<0)|(1u<<3)|(1u<<12)|(1u<<13))) return 0;
    if(!(id0&(1u<<27))||!n||n>128||(id0&255)!=n||sid>0x7fff) return 0;
    unsigned matches=0;
    for(unsigned i=0;i<n;i++) {
        if(!(smr[i]&(1u<<31))) continue;
        unsigned mask=(smr[i]>>16)&0x7fff,id=smr[i]&0x7fff;
        if(((sid^id)&~mask)!=0) continue;
        if(((s2cr[i]>>16)&3)!=1) return 0;
        matches++;
    }
    return matches==1;
}
static inline int u1_header(const uint8_t *p,uint32_t bs,uint64_t at,uint64_t blocks,
                            struct u1_gpt *g) {
    if((bs!=512&&bs!=4096)||blocks<8||at>=blocks||!u1_equal(p,"EFI PART",8)) return 0;
    uint32_t hs=u1_le32(p+12),n=u1_le32(p+80),esz=u1_le32(p+84);
    if(u1_le32(p+8)!=0x10000||hs!=92||u1_le32(p+20)||!n||n>128||esz!=128) return 0;
    if(u1_crc(p,hs,16,20)!=u1_le32(p+16)) return 0;
    struct u1_gpt x;
    x.current=u1_le64(p+24);x.other=u1_le64(p+32);x.first=u1_le64(p+40);
    x.last=u1_le64(p+48);x.entries_lba=u1_le64(p+72);x.count=n;
    x.table_crc=u1_le32(p+88);x.header_crc=u1_le32(p+16);
    for(unsigned i=0;i<16;i++) x.guid[i]=p[56+i];
    uint64_t table_blocks=((uint64_t)n*128+bs-1)/bs;
    if(x.current!=at||x.first>x.last||x.first<3||x.last>=blocks-1||
       !u1_nonzero(x.guid,16)||x.entries_lba>=blocks||table_blocks>blocks-x.entries_lba) return 0;
    if(at==1) {
        if(x.other!=blocks-1||x.entries_lba<2||x.entries_lba+table_blocks>x.first) return 0;
    } else if(at==blocks-1) {
        if(x.other!=1||x.entries_lba<=x.last||x.entries_lba+table_blocks>at) return 0;
    } else return 0;
    *g=x;return 1;
}
static inline int u1_tables(const struct u1_gpt *a,const struct u1_gpt *b,
                            const uint8_t *ta,const uint8_t *tb,struct u1_partition *part) {
    if(a->current!=1||a->other!=b->current||b->other!=1||a->first!=b->first||
       a->last!=b->last||a->count!=b->count||!a->count||a->count>128||
       a->table_crc!=b->table_crc||!u1_equal(a->guid,b->guid,16)) return 0;
    size_t bytes=(size_t)a->count*128;
    if(!u1_equal(ta,tb,bytes)||u1_crc(ta,bytes,bytes,bytes)!=a->table_crc) return 0;
    unsigned found=0;
    for(unsigned i=0;i<a->count;i++) {
        const uint8_t *e=ta+i*128;
        if(!u1_nonzero(e,16)) continue;
        uint64_t f=u1_le64(e+32),l=u1_le64(e+40);
        if(!u1_nonzero(e+16,16)||f>l||f<a->first||l>a->last) return 0;
        for(unsigned j=0;j<i;j++) {
            const uint8_t *o=ta+j*128;
            if(!u1_nonzero(o,16)) continue;
            if(u1_equal(e+16,o+16,16)||!(l<u1_le64(o+32)||f>u1_le64(o+40))) return 0;
        }
        static const char name[]="userdata";
        int match=1;
        for(unsigned j=0;j<sizeof(name);j++) if(e[56+j*2]!=(uint8_t)name[j]||e[57+j*2]) match=0;
        if(match) {
            if(++found!=1) return 0;
            part->first=f;part->last=l;
            for(unsigned j=0;j<16;j++) part->guid[j]=e[16+j];
        }
    }
    return found==1;
}
#endif
