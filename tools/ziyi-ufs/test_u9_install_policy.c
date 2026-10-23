#include "u9_install_policy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d\n",__LINE__);exit(1);}}while(0)
static void be(uint8_t *p,uint32_t x){p[0]=x>>24;p[1]=x>>16;p[2]=x>>8;p[3]=x;}
int main(void){
 uint8_t h[512]={0},id[28],other[28],b[512],r[4]={0};
 h[0]='H';h[1]='X';h[3]=5;be(h+4,0x2100);be(h+12,8);be(h+16,12345);
 be(h+40,4096);be(h+44,2097152);be(h+104,0x1234);be(h+108,0x5678);
 CHECK(u9_hfs_identity(h,512,8ull<<30,id));
 for(unsigned i=0;i<512;i++){
   memcpy(b,h,512);b[i]^=0x80;
   int ok=u9_hfs_identity(b,512,8ull<<30,other);
   int stable=(i<4)||(i>=12&&i<20)||(i>=40&&i<48)||(i>=104&&i<112);
   CHECK(stable?(!ok||memcmp(id,other,28)!=0):(ok&&memcmp(id,other,28)==0));
 }
 for(unsigned n=0;n<512;n++)CHECK(!u9_hfs_identity(h,n,8ull<<30,id));
 CHECK(!u9_hfs_identity(h,512,3ull<<30,id));
 memcpy(b,h,512);memset(b+104,0,8);CHECK(!u9_hfs_identity(b,512,8ull<<30,id));
 for(unsigned flags=0;flags<16;flags++)CHECK(u9_sync_valid(8ull<<30,0,0,flags,r)==(flags==0||flags==2));
 CHECK(u9_sync_valid(8ull<<30,(8ull<<30)-4096,4096,2,r));
 CHECK(!u9_sync_valid(8ull<<30,UINT64_MAX,1,0,r));
 CHECK(!u9_sync_valid(8ull<<30,(8ull<<30)-4096,4097,0,r));
 for(unsigned i=0;i<4;i++){r[i]=1;CHECK(!u9_sync_valid(8ull<<30,0,0,0,r));r[i]=0;}
 printf("U9_INSTALL_IDENTITY_AND_SYNC_PASS checks=%u\n",checks);return 0;
}
