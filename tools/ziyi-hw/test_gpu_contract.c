#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ziyi_gpu_probe_contract.h"
#include "contract-fixture.h"
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"GPU contract line %u\n",__LINE__);return 1;}}while(0)
static int get(void*ctx,const char*name,const void**value,unsigned*bytes){(void)ctx;for(unsigned i=0;i<sizeof(fixture)/sizeof(fixture[0]);i++)if(!strcmp(fixture[i].name,name)){if(fixture[i].hidden)return 0;*value=fixture[i].value;*bytes=fixture[i].bytes;return 1;}return 0;}
int main(void){
 struct zg_contract c;CHECK(zg_contract_read(&c,get,0));CHECK(c.board==0x06060201&&c.cb==6&&c.smr==6&&c.sid==5&&c.sid_mask==0x400&&!c.handoff);
 for(unsigned i=0;i<sizeof(fixture)/sizeof(fixture[0]);i++){
  unsigned n=fixture[i].bytes;fixture[i].hidden=1;CHECK(!zg_contract_read(&c,get,0));fixture[i].hidden=0;
  fixture[i].bytes=n-1;CHECK(!zg_contract_read(&c,get,0));fixture[i].bytes=n;
  for(unsigned j=0;j<n;j++){fixture[i].value[j]^=1;CHECK(!zg_contract_read(&c,get,0));fixture[i].value[j]^=1;}
 }
 CHECK(zg_contract_read(&c,get,0));
 for(unsigned i=0;i<4;i++)for(unsigned pg=4096;pg<=65536;pg*=2){
  struct zg_geometry g;CHECK(zg_map_geometry(c.regions[i].base,c.regions[i].bytes,pg,&g));CHECK(!(g.page_base&(pg-1))&&!(g.page_span&(pg-1))&&g.bias<pg&&g.page_base+g.bias==c.regions[i].base&&g.bias+c.regions[i].bytes<=g.page_span);
  uint64_t off;CHECK(zg_region_offset(&c.regions[i],c.regions[i].base,4,&off)&&off==0);CHECK(zg_region_offset(&c.regions[i],c.regions[i].base+c.regions[i].bytes-4,4,&off));CHECK(!zg_region_offset(&c.regions[i],c.regions[i].base+c.regions[i].bytes-3,4,&off));CHECK(!zg_region_offset(&c.regions[i],c.regions[i].base-4,4,&off));
 }
 struct zg_geometry g;CHECK(!zg_map_geometry(UINT64_MAX-3,8,4096,&g));CHECK(!zg_map_geometry(0,UINT64_MAX,4096,&g));CHECK(!zg_map_geometry(0,0,4096,&g));CHECK(!zg_map_geometry(0,4,3,&g));
 CHECK(zg_kernel_context(4,0x1005,19200000));for(unsigned b=0;b<64;b++)if((b==0)||(b==2)||(b==12)||(b==24)||(b==25))CHECK(!zg_kernel_context(4,0x1005^(1ULL<<b),19200000));CHECK(!zg_kernel_context(8,0x1005,19200000));CHECK(!zg_kernel_context(4,0x1005,24000000));
 printf("GPU_ACTUAL_CONTRACT_HOST_PASS checks=%u\n",checks);return 0;
}
