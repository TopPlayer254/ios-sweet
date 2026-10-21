/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "u1_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++;if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);} }while(0)
static void le32(uint8_t *p,uint32_t n) { for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i)); }
static void le64(uint8_t *p,uint64_t n) { le32(p,(uint32_t)n);le32(p+4,(uint32_t)(n>>32)); }
static uint8_t hdr[2][4096],tbl[2][16384];
static void seal(void) {
    memcpy(tbl[1],tbl[0],sizeof(tbl[0]));
    for(unsigned i=0;i<2;i++) {
        le32(hdr[i]+88,u1_crc(tbl[i],4096,4096,4096));
        le32(hdr[i]+16,u1_crc(hdr[i],92,16,20));
    }
}
static void fixture(unsigned bs) {
    memset(hdr,0,sizeof(hdr));memset(tbl,0,sizeof(tbl));
    for(unsigned i=0;i<2;i++) {
        memcpy(hdr[i],"EFI PART",8);le32(hdr[i]+8,0x10000);le32(hdr[i]+12,92);
        le64(hdr[i]+24,i?127:1);le64(hdr[i]+32,i?1:127);
        le64(hdr[i]+40,10);le64(hdr[i]+48,118);hdr[i][56]=7;
        le64(hdr[i]+72,i?127-4096/bs:2);le32(hdr[i]+80,32);le32(hdr[i]+84,128);
    }
    tbl[0][0]=1;tbl[0][16]=2;le64(tbl[0]+32,20);le64(tbl[0]+40,99);
    for(unsigned j=0;j<8;j++) tbl[0][56+2*j]=(uint8_t)"userdata"[j];
    seal();
}
static int parse(unsigned bs) {
    struct u1_gpt a,b;struct u1_partition p;
    if(!u1_header(hdr[0],bs,1,128,&a)||!u1_header(hdr[1],bs,127,128,&b)) return 0;
    return u1_tables(&a,&b,tbl[0],tbl[1],&p)&&p.first==20&&p.last==99&&p.guid[0]==2;
}
int main(void) {
    CHECK(u1_crc((const uint8_t *)"123456789",9,9,9)==0xcbf43926);
    struct u1_cmd c;
    for(unsigned op=0;op<256;op++) {
        int ok=u1_command(&c,op,0,0,0,0,0);
        CHECK(ok==(op==0||op==0x25||op==0x9e));
    }
    CHECK(u1_command(&c,0x28,7,127,1,4096,128)&&c.bytes==4096&&c.cdb[5]==127&&c.cdb[8]==1);
    CHECK(!u1_command(&c,0x28,8,0,1,4096,128));
    CHECK(!u1_command(&c,0x28,0,128,1,4096,128));
    CHECK(!u1_command(&c,0x28,0,127,2,4096,128));
    CHECK(!u1_command(&c,0x28,0,0,0,4096,128));
    CHECK(!u1_command(&c,0x28,0,0,9,512,128));
    CHECK(!u1_command(&c,0x28,0,0,1,1024,128));
    CHECK(!u1_command(&c,0x28,0,UINT64_MAX,1,512,UINT64_MAX));
    CHECK(!u1_command(&c,0x28,0,UINT32_MAX,2,512,(uint64_t)UINT32_MAX+2));
    CHECK(u1_command(&c,0x9e,0,0,0,0,0)&&c.bytes==32&&c.cdb[1]==16&&c.cdb[13]==32);
    uint8_t cap[32]={0};uint64_t blocks;uint32_t bs;
    u1_putbe32(cap,127);u1_putbe32(cap+4,4096);
    CHECK(u1_capacity(cap,8,&blocks,&bs)&&blocks==128&&bs==4096);
    CHECK(!u1_capacity(cap,7,&blocks,&bs));
    u1_putbe32(cap,UINT32_MAX);CHECK(!u1_capacity(cap,8,&blocks,&bs));
    memset(cap,0,sizeof(cap));u1_putbe32(cap+4,127);u1_putbe32(cap+8,512);
    CHECK(u1_capacity(cap,32,&blocks,&bs)&&blocks==128&&bs==512);
    u1_putbe32(cap+8,8192);CHECK(!u1_capacity(cap,32,&blocks,&bs));
    uint32_t smr[2]={0x80000020,0},s2cr[2]={1u<<16,0},id=(1u<<27)|2;
    CHECK(u1_smmu_bypass(0,id,smr,s2cr,2,0x20));
    for(unsigned t=0;t<4;t++) {s2cr[0]=t<<16;CHECK(u1_smmu_bypass(0,id,smr,s2cr,2,0x20)==(t==1));}
    s2cr[0]=1u<<16;
    CHECK(!u1_smmu_bypass(1,id,smr,s2cr,2,0x20));
    CHECK(!u1_smmu_bypass(8,id,smr,s2cr,2,0x20));
    CHECK(!u1_smmu_bypass(1u<<12,id,smr,s2cr,2,0x20));
    CHECK(!u1_smmu_bypass(1u<<13,id,smr,s2cr,2,0x20));
    CHECK(!u1_smmu_bypass(0,2,smr,s2cr,2,0x20));
    CHECK(!u1_smmu_bypass(0,id,smr,s2cr,1,0x20));
    smr[0]=0;CHECK(!u1_smmu_bypass(0,id,smr,s2cr,2,0x20));
    smr[0]=smr[1]=0x80000020;s2cr[1]=1u<<16;
    CHECK(!u1_smmu_bypass(0,id,smr,s2cr,2,0x20));
    smr[1]=0;smr[0]=0x800f0020;CHECK(u1_smmu_bypass(0,id,smr,s2cr,2,0x25));
    for(unsigned size=512;size<=4096;size*=8) {
        fixture(size);CHECK(parse(size));
        for(unsigned i=0;i<92;i++) { hdr[0][i]^=0x80;CHECK(!parse(size));hdr[0][i]^=0x80; }
        tbl[1][0]^=1;CHECK(!parse(size));fixture(size);
        tbl[0][0]^=1;CHECK(!parse(size));fixture(size);
        le32(hdr[0]+80,UINT32_MAX);seal();CHECK(!parse(size));fixture(size);
        le32(hdr[0]+84,UINT32_MAX);seal();CHECK(!parse(size));fixture(size);
        le32(hdr[0]+12,4097);seal();CHECK(!parse(size));fixture(size);
        le64(hdr[0]+72,UINT64_MAX);seal();CHECK(!parse(size));fixture(size);
        le64(hdr[1]+72,0);seal();CHECK(!parse(size));fixture(size);
        le64(hdr[1]+32,2);seal();CHECK(!parse(size));fixture(size);
        hdr[1][56]=8;seal();CHECK(!parse(size));fixture(size);
        le64(tbl[0]+32,9);seal();CHECK(!parse(size));fixture(size);
        le64(tbl[0]+40,128);seal();CHECK(!parse(size));fixture(size);
        le64(tbl[0]+40,19);seal();CHECK(!parse(size));fixture(size);
        memset(tbl[0]+16,0,16);seal();CHECK(!parse(size));fixture(size);
        memcpy(tbl[0]+128,tbl[0],128);tbl[0][144]=3;
        le64(tbl[0]+128+32,100);le64(tbl[0]+128+40,110);seal();CHECK(!parse(size));fixture(size);
        memcpy(tbl[0]+128,tbl[0],128);tbl[0][144]=3;tbl[0][128+56]='x';
        seal();CHECK(!parse(size));fixture(size);
        tbl[0][57]=1;seal();CHECK(!parse(size));
    }
    printf("U1_CORE_PASS checks=%u (commands, bounds, SMMU, dual GPT CRC, corruption)\n",checks);
    return 0;
}
