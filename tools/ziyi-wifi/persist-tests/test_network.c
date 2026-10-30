/* SPDX-License-Identifier: MIT */
#include "ziyi_wifi_network.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
 unsigned checks=0;uint8_t p[ZNP_MAX+1];
 for(unsigned ssid=1;ssid<=32;ssid++)for(unsigned password=8;password<=63;password++){
  unsigned n=ssid+password+2;memset(p,'x',n);p[ssid]='\n';p[n-1]='\n';
  assert(znp_valid(p,n));checks++;
  for(unsigned at=0;at<n;at++){
   uint8_t old=p[at];p[at]=0;assert(!znp_valid(p,n));checks++;p[at]=old;
  }
 }
 memset(p,'x',sizeof(p));assert(!znp_valid(NULL,11));checks++;
 for(unsigned n=0;n<=sizeof(p);n++){assert(!znp_valid(p,n));checks++;}
 for(unsigned byte=0;byte<=255;byte++){
  uint8_t q[]="test\npassword\n";q[1]=(uint8_t)byte;
  assert(znp_valid(q,sizeof(q)-1)==(byte>=32&&byte<=126));checks++;
 }
 assert(!znp_valid((const uint8_t*)"\npassword\n",10));checks++;
 assert(!znp_valid((const uint8_t*)"test\npass\n",10));checks++;
 assert(!znp_valid((const uint8_t*)"test\npassword\nextra\n",20));checks++;
 printf("ZIYI_NETWORK_FORMAT_PASS %u checks; synthetic data only\n",checks);
 return 0;
}
