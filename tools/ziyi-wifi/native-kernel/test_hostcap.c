#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "hostcap.inc"
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"hostcap line %d\n",__LINE__);exit(1);}}while(0)
static const uint8_t request[]={0,0x42,0,0x34,0,7,0,0x10,4,0,1,0,0,0};
static const uint8_t expected[]={0x2d,17,0,4,25,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0};
static void reject(uint8_t *b,size_t n,size_t cap,unsigned gpio,int bound)
{uint8_t old[400];memcpy(old,b,400);CHECK(ziyi_wlfw_host_cap(b,n,cap,gpio,bound)<0);CHECK(!memcmp(old,b,400));}
int main(void)
{
 uint8_t b[400],old[400];memset(b,0xa5,sizeof(b));memcpy(b,request,sizeof(request));
 CHECK(ziyi_wlfw_host_cap(b,sizeof(request),sizeof(b),25,1)==34);
 CHECK(b[5]==27&&b[6]==0);CHECK(!memcmp(b,request,5));
 CHECK(!memcmp(b+7,request+7,sizeof(request)-7));CHECK(!memcmp(b+14,expected,20));
 CHECK(b[34]==0xa5);reject(b,34,sizeof(b),25,1); /* duplicate rejects unchanged */
 for(unsigned n=0;n<sizeof(request);n++){
  memset(b,0xa5,sizeof(b));memcpy(b,request,sizeof(request));reject(b,n,sizeof(b),25,1);
 }
 for(unsigned cap=0;cap<34;cap++){
  memset(b,0xa5,sizeof(b));memcpy(b,request,sizeof(request));reject(b,14,cap,25,1);
 }
 for(unsigned v=0;v<256;v++)if(v!=25){
  memset(b,0xa5,sizeof(b));memcpy(b,request,sizeof(request));reject(b,14,sizeof(b),v,1);
 }
 memcpy(b,request,sizeof(request));reject(b,14,sizeof(b),25,0);
 for(unsigned i=0;i<14;i++){
  if(i==1||i==2||i==7||i>=10)continue; /* transaction/value bytes are preserved */
  memcpy(b,request,sizeof(request));b[i]^=0x80;reject(b,14,sizeof(b),25,1);
 }
 /* Boundary permits exact capacity, leaves every existing TLV unchanged. */
 for(unsigned n=0;n<=255;n++){
  size_t len=10+n;memset(b,0xa5,sizeof(b));memcpy(b,request,7);
  b[5]=(uint8_t)(len-7);b[6]=(uint8_t)((len-7)>>8);b[7]=0x60;b[8]=(uint8_t)n;b[9]=0;
  memcpy(old,b,sizeof(b));CHECK(ziyi_wlfw_host_cap(b,len,len+20,25,1)==(int)len+20);
  CHECK(!memcmp(b,old,5));CHECK(!memcmp(b+7,old+7,len-7));CHECK(!memcmp(b+len,expected,20));
 }
 CHECK(ziyi_wlfw_host_cap(NULL,7,27,25,1)<0);
 printf("ZIYI_HOST_CAP_GPIO_PASS %u checks; wire bytes, rejection atomicity and bounds\n",checks);
 return 0;
}
