/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "u4-sha-test.inc"
#include "u4_verify.h"
static unsigned checks;
#define CHECK(x) do { checks++;if(!(x)){fprintf(stderr,"FAIL line %d\n",__LINE__);exit(1);} }while(0)
int main(int argc,char **argv) {
    uint8_t data[4096],actual[32],expected[32];struct u4_hash s;
    for(unsigned i=0;i<4096;i++) data[i]=(uint8_t)(i*17+i/31);
    for(unsigned n=64;n<=4096;n+=64) {
        u4_hash_init(&s);CHECK(u4_hash_blocks(&s,data,n));
        CHECK(u4_hash_final(&s,n,actual));s2_sha256(data,n,expected);CHECK(!memcmp(actual,expected,32));
        CHECK(!u4_hash_final(&s,n,actual));
    }
    u4_hash_init(&s);CHECK(!u4_hash_blocks(&s,data,63));CHECK(!u4_hash_final(&s,0,actual));
    u4_hash_init(&s);CHECK(!u4_hash_blocks(&s,data,4097));
    u4_hash_init(&s);s.bytes=UINT64_MAX/8-63;CHECK(!u4_hash_blocks(&s,data,64));
    u4_hash_init(&s);CHECK(u4_hash_blocks(&s,data,4096));CHECK(!u4_hash_final(&s,8192,actual));
    CHECK(u4_expected_hash("3182c24764d970c2bfe7bec5c6f73f07b79aae602047ab9aae29f79756532006",expected));
    CHECK(!u4_expected_hash("Z182c24764d970c2bfe7bec5c6f73f07b79aae602047ab9aae29f79756532006",expected));
    if(argc==2) {
        FILE *f=fopen(argv[1],"rb");CHECK(f!=NULL);u4_hash_init(&s);
        size_t n;while((n=fread(data,1,sizeof(data),f))) {if(!u4_hash_blocks(&s,data,n))return 2;}
        CHECK(!ferror(f));CHECK(fclose(f)==0);CHECK(s.bytes==3221225472ull);
        CHECK(u4_hash_final(&s,3221225472ull,actual));
        CHECK(u4_expected_hash("3182c24764d970c2bfe7bec5c6f73f07b79aae602047ab9aae29f79756532006",expected));
        CHECK(!memcmp(actual,expected,32));
        printf("U4_REAL_3GIB_SHA_PASS ");
    }
    printf("U4_HASH_PASS checks=%u\n",checks);return 0;
}
