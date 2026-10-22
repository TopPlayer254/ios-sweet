#include "u7_write_policy.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL line %d\n",__LINE__);exit(1);}}while(0)
int main(void){
    struct u1_cmd c;struct u7_volume v={1000,100000000,3ull<<30,4096,0,1,1,1};
    CHECK(u7_write_command(&c,&v,1000,1)&&c.cdb[0]==0x2a&&c.cdb[1]==8&&c.bytes==4096);
    for(unsigned size=3;size<=8;size+=5)for(unsigned bs=512;bs<=4096;bs*=8){
        v.image_bytes=(uint64_t)size<<30;v.physical=bs;uint64_t end=v.first+v.image_bytes/bs;
        CHECK(u7_write_command(&c,&v,v.first,1));CHECK(u7_write_command(&c,&v,end-1,1));
        CHECK(!u7_write_command(&c,&v,v.first-1,1));CHECK(!u7_write_command(&c,&v,end,1));
        CHECK(!u7_write_command(&c,&v,end-1,2));CHECK(!u7_write_command(&c,&v,v.first,0));
        CHECK(u7_write_command(&c,&v,v.first,4096/bs));CHECK(!u7_write_command(&c,&v,v.first,4096/bs+1));
        for(unsigned a=0;a<4;a++)for(unsigned b=0;b<4;b++)for(unsigned d=0;d<4;d++){
            v.root_verified=a;v.gpt_verified=b;v.writable=d;
            CHECK(u7_write_command(&c,&v,v.first,1)==(a==1&&b==1&&d==1));
        }
        v.root_verified=v.gpt_verified=v.writable=1;
    }
    for(unsigned lun=0;lun<256;lun++){v.lun=lun;CHECK(u7_write_command(&c,&v,v.first,1)==(lun<8));}
    v.lun=0;v.first=UINT64_MAX;CHECK(!u7_volume_valid(&v));v.first=1000;v.last=UINT64_MAX;CHECK(!u7_volume_valid(&v));
    v.last=1000;CHECK(!u7_volume_valid(&v));v.last=100000000;v.image_bytes=7ull<<30;CHECK(!u7_volume_valid(&v));
    v.image_bytes=8ull<<30;v.physical=1024;CHECK(!u7_volume_valid(&v));v.physical=4096;
    CHECK(!u7_write_command(&c,&v,UINT64_MAX,1));CHECK(!u7_write_command(&c,&v,UINT32_MAX,2));
    printf("U7_PREFIX_WRITE_POLICY_PASS checks=%u; NOT PHONE INTEGRATED\n",checks);return 0;
}
