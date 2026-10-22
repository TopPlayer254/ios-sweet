#include "u6_transport_policy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d\n",__LINE__);exit(1);}}while(0)
static void put(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
int main(void){
    uint8_t r[256]={0},u[96]={0};uint64_t base=0xff137000;
    memset(r+32,0xaf,224);put(r,0x1000000);put(r+16,(uint32_t)(base+256));put(r+24,0x80010);put(r+28,0x180000);
    u[0]=1;u[2]=0xd0;u[3]=0x9f;u[16]=0x1b;u[20]=0x30;u[32]=0x21;u[34]=0xd0;u[35]=0x9f;
    CHECK(u6_firmware_profile(r,u,base));CHECK(!u6_firmware_profile(r,u,base+1));CHECK(!u6_firmware_profile(r,u,0xf0000000));
    for(unsigned i=0;i<256;i++)for(unsigned b=0;b<8;b++){r[i]^=1u<<b;CHECK(!u6_firmware_profile(r,u,base));r[i]^=1u<<b;}
    for(unsigned i=0;i<64;i++)for(unsigned b=0;b<8;b++){u[i]^=1u<<b;CHECK(!u6_firmware_profile(r,u,base));u[i]^=1u<<b;}
    u[3]=u[35]=17;CHECK(u6_firmware_profile(r,u,base));
    struct u1_cmd c;
    for(unsigned op=0;op<256;op++){
        CHECK(u6_command(&c,op,0xd0,0,0,0,0)==(op==0||op==3||op==0x1b));
        CHECK(u6_command(&c,op,0,0,0,0,0)==(op==0||op==0x25||op==0x9e));
        CHECK(!u6_command(&c,op,8,0,0,0,0));
    }
    CHECK(u6_command(&c,0x1b,0xd0,0,0,0,0)&&c.bytes==0&&c.cdb[4]==0x10);
    for(unsigned i=1;i<16;i++)if(i!=4)CHECK(c.cdb[i]==0);
    CHECK(!u6_command(&c,0x1b,0xd0,1,0,0,0));CHECK(!u6_command(&c,0x1b,0xd0,0,1,0,0));
    CHECK(u6_command(&c,3,0xd0,0,0,0,0)&&c.bytes==18&&c.cdb[4]==18);
    printf("U6_PROFILE_WAKE_PASS checks=%u\n",checks);return 0;
}
