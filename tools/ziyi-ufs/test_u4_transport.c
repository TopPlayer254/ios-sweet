/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "u4_transport_policy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++;if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);} }while(0)
static void le32(uint8_t *p,uint32_t v) { for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i)); }
static struct u3_smmu good(void) {
    struct u3_smmu s={0};s.cr0=0x280406;s.id0=0x4c017e7d;s.id1=0x6000004e;s.id2=0x5111;s.id7=0x24;
    s.smr[0]=0x80000020;s.cbar=0x1f000;s.cba2r=1;s.sctlr=0x0a9f00e0;s.tcr2=0x60;return s;
}
int main(void) {
    struct u3_smmu s=good();CHECK(u3_identity(&s));s.cr0=0x290406;CHECK(u3_identity(&s));
    /* Every single-bit change of a live profile/control value must be rejected,
     * except the measured CR0 memory-attribute variant. */
    uint32_t *fields[]={&s.cr0,&s.cr2,&s.id0,&s.id1,&s.id2,&s.id7,&s.smr[0],&s.s2cr[0],
        &s.cbar,&s.cba2r,&s.sctlr,&s.tcr2,&s.ttbr0lo,&s.ttbr0hi,&s.ttbr1lo,&s.ttbr1hi,&s.tcr};
    for(unsigned j=0;j<sizeof(fields)/sizeof(fields[0]);j++)for(unsigned b=0;b<32;b++) {
        *fields[j]^=1u<<b;CHECK(u3_identity(&s)==(j==0&&b==16));*fields[j]^=1u<<b;
    }
    for(unsigned i=1;i<U3_MATCHES;i++) {s.smr[i]=0x80000020;CHECK(!u3_identity(&s));s.smr[i]=0;}
    CHECK(u3_dma_range(&s,0x800000000ull,0x8000,1u<<24));
    CHECK(!u3_dma_range(&s,0x800000000ull,0x8000,0));
    CHECK(!u3_dma_range(&s,UINT64_MAX,2,~0u));CHECK(!u3_dma_range(&s,1ull<<36,1,~0u));
    /* Unequal IAS/OAS specifically catches U1's swapped nibble bug. */
    s.id2=0x12;CHECK(!u3_dma_range(&s,1ull<<38,1,~0u));
    s.id2=0x21;CHECK(!u3_dma_range(&s,1ull<<38,1,~0u));
    CHECK(u3_firmware_span(0xf1400000,4096));CHECK(!u3_firmware_span(0x100000000ull,1));
    CHECK(!u3_firmware_span(0xfffff000,4097));CHECK(!u3_firmware_span(0x800000000ull,4096));
    uint8_t d[32]={0},u[4096]={0};uint64_t pa;unsigned poff,n;
    le32(d,(1u<<28)|(2u<<25));le32(d+16,0xff135000);le32(d+24,(128u<<16)|128u);le32(d+28,(256u<<16)|1);
    CHECK(u3_old_utrd(d,&pa,&poff,&n)&&pa==0xff135000&&poff==1024&&n==1);
    u[0]=1;u[1]=0x40;u[16]=0x28;u1_putbe32(u+12,4096);
    le32(u+poff,0xff140000);le32(u+poff+12,4095);CHECK(u3_old_ucd(u,poff,n));
    for(unsigned op=0;op<256;op++) {
        u[16]=(uint8_t)op;
        CHECK(u3_old_ucd(u,poff,n)==(op==0||op==0x12||op==3||op==0x25||op==0x28||op==0x88||op==0xa0));
    }
    u[16]=0x28;u[1]=0x20;CHECK(!u3_old_ucd(u,poff,n));u[1]=0x40;
    le32(u+poff+4,8);CHECK(!u3_old_ucd(u,poff,n));le32(u+poff+4,0);
    le32(u+poff+12,0x40000);CHECK(!u3_old_ucd(u,poff,n));le32(u+poff+12,4095);
    le32(d,(1u<<28)|(1u<<25));CHECK(!u3_old_utrd(d,&pa,&poff,&n));
    memset(d,0,sizeof(d));CHECK(u4_inert_slot(d));d[8]=15;CHECK(u4_inert_slot(d));
    d[8]=1;CHECK(!u4_inert_slot(d));d[8]=0;d[16]=1;CHECK(!u4_inert_slot(d));
    memset(d,0,sizeof(d));le32(d,1u<<24);le32(d+16,0xff12f100);
    le32(d+24,(8u<<16)|8u);CHECK(u3_old_utrd(d,&pa,&poff,&n)&&n==0);
    memset(u,0,sizeof(u));CHECK(u3_old_ucd(u,poff,n));
    u[3]=17;CHECK(u3_old_ucd(u,poff,n));u[12]=1;CHECK(!u3_old_ucd(u,poff,n));u[12]=0;
    u[0]=1;CHECK(u3_old_ucd(u,poff,n));u[16]=0x2a;CHECK(!u3_old_ucd(u,poff,n));
    le32(d,(2u<<28));CHECK(!u3_old_utrd(d,&pa,&poff,&n));
    uint64_t off,bytes;
    CHECK(u3_partition_io(100,199,4096,512,799,512,&off,&bytes)&&off==409088&&bytes==409600);
    CHECK(!u3_partition_io(100,199,4096,512,800,1,&off,&bytes));
    CHECK(u3_partition_io(100,199,4096,512,800,0,&off,&bytes));
    CHECK(!u3_partition_io(100,199,4096,512,-1,1,&off,&bytes));
    CHECK(!u3_partition_io(100,199,4096,1023,0,1,&off,&bytes));
    CHECK(!u3_partition_io(0,UINT64_MAX,4096,512,0,1,&off,&bytes));
    CHECK(!u3_partition_io(0,UINT64_MAX/4096+1,4096,512,0,1,&off,&bytes));
    CHECK(!u3_partition_io(100,199,4096,512,INT64_MAX,1,&off,&bytes));
    CHECK(!u3_partition_io(100,199,4096,512,0,U3_REQUEST_MAX+1,&off,&bytes));
    printf("U4_TRANSPORT_POLICY_PASS checks=%u\n",checks);return 0;
}
