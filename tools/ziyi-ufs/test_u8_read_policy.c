#include "u8_read_policy.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d\n",__LINE__);exit(1);}}while(0)
int main(void){
    struct u1_cmd c;
    CHECK(U8_READ_MAX==16384&&U1_DATA_OFF+U8_READ_MAX==U1_DMA_BYTES);
    for(unsigned bs=512;bs<=4096;bs*=8)for(unsigned lun=0;lun<8;lun++){
        for(unsigned n=1;n<=U8_READ_MAX/bs;n++){
            CHECK(u8_command(&c,0x28,lun,100,n,bs,100000));
            CHECK(c.bytes==n*bs&&c.bytes<=U8_READ_MAX&&c.cdb[0]==0x28&&c.lun==lun);
            CHECK(c.cdb[7]==0&&c.cdb[8]==n&&u1_be32(c.cdb+2)==100);
            CHECK(u8_command(&c,0x28,lun,100000-n,n,bs,100000));
            CHECK(!u8_command(&c,0x28,lun,100001-n,n,bs,100000));
        }
        CHECK(!u8_command(&c,0x28,lun,0,0,bs,100000));
        CHECK(!u8_command(&c,0x28,lun,0,U8_READ_MAX/bs+1,bs,100000));
    }
    for(unsigned op=0;op<256;op++){
        CHECK(!u8_command(&c,op,0,100,4,4096,100000)||(op==0x28));
        CHECK(!u8_command(&c,op,0xd0,100,4,4096,100000));
    }
    CHECK(!u8_command(&c,0x28,8,0,1,4096,100000));
    CHECK(!u8_command(&c,0x28,0,UINT32_MAX,2,4096,UINT64_MAX));
    CHECK(!u8_command(&c,0x28,0,UINT64_MAX,1,4096,UINT64_MAX));
    CHECK(!u8_command(&c,0x28,0,0,1,1024,100000));
    CHECK(u8_command(&c,0,0xd0,0,0,0,0));
    CHECK(u8_command(&c,0x1b,0xd0,0,0,0,0)&&c.cdb[4]==0x10);
    printf("U8_BOUNDED_READ_BATCH_PASS checks=%u maxBytes=%u\n",checks,U8_READ_MAX);return 0;
}
