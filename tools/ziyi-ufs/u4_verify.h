/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Streaming SHA for block-aligned readback, sharing the already audited SHA
 * compression function Block(). This is a gate for future Apple root entry.
 */
#ifndef ZIYI_U4_VERIFY_H
#define ZIYI_U4_VERIFY_H
struct u4_hash { uint32_t h[8];uint64_t bytes;int failed; };
static inline void u4_hash_init(struct u4_hash *s) {
    static const uint32_t iv[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    for(unsigned i=0;i<8;i++) s->h[i]=iv[i];s->bytes=0;s->failed=0;
}
static inline int u4_hash_blocks(struct u4_hash *s,const uint8_t *p,size_t n) {
    if(s->failed||!n||n>4096||n%64||s->bytes>UINT64_MAX/8-n) {s->failed=1;return 0;}
    for(size_t i=0;i<n;i+=64) Block(s->h,p+i);
    s->bytes+=n;return 1;
}
static inline int u4_hash_final(struct u4_hash *s,uint64_t expected,uint8_t out[32]) {
    if(s->failed||s->bytes!=expected||expected%64) return 0;
    uint8_t tail[64]={0x80};uint64_t bits=s->bytes*8;
    for(unsigned i=0;i<8;i++) tail[63-i]=(uint8_t)(bits>>(8*i));
    Block(s->h,tail);
    for(unsigned i=0;i<32;i++) out[i]=(uint8_t)(s->h[i/4]>>(24-8*(i%4)));
    s->failed=1;return 1;
}
static inline int u4_expected_hash(const char *hex,uint8_t out[32]) {
    for(unsigned i=0;i<64;i++) {
        unsigned c=(unsigned char)hex[i];unsigned v;
        if(c>='0'&&c<='9') v=c-'0';else if(c>='a'&&c<='f') v=c-'a'+10;else return 0;
        if(i%2) out[i/2]|=(uint8_t)v;else out[i/2]=(uint8_t)(v<<4);
    }
    return hex[64]==0;
}
#endif
