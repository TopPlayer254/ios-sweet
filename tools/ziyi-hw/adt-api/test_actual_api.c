/* SPDX-License-Identifier: MIT
 * Calls the unchanged Apple 8796 device_tree.c and its real device_tree.h.
 * Only the host physical translation provider is simulated for CTRR tests.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <pexpert/device_tree.h>
#include "ziyi_adt_binding.h"
#include "ziyi_gpu_adt_properties.inc"
#include "ziyi_wifi_adt_properties.inc"
#include "ziyi_gpu_probe_contract.h"
#include "ziyi_wifi_contract.h"
static unsigned tests;
static uint8_t*tree;static size_t tree_bytes;
static int protected_before=1;
char _mh_execute_header;
uint64_t kvtophys(uintptr_t address){
 if(address==(uintptr_t)&_mh_execute_header)return 0x200000;
 if(address>=(uintptr_t)tree&&address<=(uintptr_t)tree+tree_bytes)return (protected_before?0x100000:0x300000)+(address-(uintptr_t)tree);
 abort();
}
void panic(const char*message,...){va_list ap;va_start(ap,message);vfprintf(stderr,message,ap);va_end(ap);fputc('\n',stderr);abort();}
#define CHECK(x) do{tests++;if(!(x)){fprintf(stderr,"Actual ADT API test line%u failed\n",__LINE__);exit(1);}}while(0)
static int actual_gpu_get(void*entry,const char*name,const void**data,unsigned*bytes){return SecureDTGetProperty((DTEntry)entry,name,data,bytes)==kSuccess;}
static void exercise(const char*path,const struct zadt_property*props,unsigned count){
 DTEntry entry=0;CHECK(SecureDTLookupEntry(NULL,path,&entry)==kSuccess);CHECK(entry!=NULL);
 const void*value=0;unsigned bytes=0;CHECK(SecureDTGetProperty(entry,"compatible",&value,&bytes)==kSuccess);
 CHECK(kSuccess==1);CHECK(kIterationDone==0);CHECK(kError==-1);
 struct zadt_binding binding;
#if !SWEET && !ZADT_LOCKDOWN_AVAILABLE
 CHECK(!zadt_open(&binding,path,props,count));CHECK(binding.status==ZADT_PLATFORM);return;
#endif
 CHECK(zadt_open(&binding,path,props,count));CHECK(binding.passes==2);CHECK(binding.ready);CHECK(binding.available==ZADT_LOCKDOWN_AVAILABLE);
#if ZADT_LOCKDOWN_AVAILABLE
 CHECK(SecureDTIsLockedDown());CHECK(binding.observed==1&&binding.mode==ZADT_APPLE_PROTECTED);
 protected_before=0;CHECK(!SecureDTIsLockedDown());CHECK(!zadt_recheck(&binding,path,props,count));CHECK(binding.status==ZADT_NOT_PROTECTED);CHECK(!binding.ready);protected_before=1;
 CHECK(zadt_open(&binding,path,props,count));
#else
 CHECK(!SecureDTIsLockedDown());CHECK(binding.observed==0&&binding.mode==ZADT_SWEET_BOARD_SNAPSHOT);
#endif
 CHECK(zadt_recheck(&binding,path,props,count));CHECK(binding.passes==3);
 CHECK(!zadt_open(&binding,"/arm-io/missing-probe",props,count));CHECK(binding.status==ZADT_NODE);
 CHECK(zadt_open(&binding,path,props,count));binding.entry=NULL;CHECK(!zadt_recheck(&binding,path,props,count));
 for(unsigned i=0;i<count;i++){
  CHECK(SecureDTGetProperty(entry,props[i].name,&value,&bytes)==kSuccess);CHECK(bytes==props[i].bytes);
  uint8_t*p=(uint8_t*)(uintptr_t)value;
  for(unsigned j=0;j<bytes;j++){
   p[j]^=1;CHECK(!zadt_open(&binding,path,props,count));p[j]^=1;
  }
  CHECK(zadt_open(&binding,path,props,count));
  /* Alter length without changing its rounded4byte footprint: still a valid serialized tree. */
  DeviceTreeNodeProperty*header=(DeviceTreeNodeProperty*)(p-sizeof(DeviceTreeNodeProperty));
  unsigned original=header->length;header->length=original%4==1?original+1:original-1;
  CHECK(!zadt_open(&binding,path,props,count));header->length=original;
  char first=header->name[0];header->name[0]='!';CHECK(!zadt_open(&binding,path,props,count));header->name[0]=first;
  CHECK(zadt_open(&binding,path,props,count));
 }
 CHECK(zadt_open(&binding,path,props,count));
 CHECK(SecureDTGetProperty(entry,"compatible",&value,&bytes)==kSuccess);
 uint8_t*p=(uint8_t*)(uintptr_t)value;p[0]^=1;CHECK(!zadt_recheck(&binding,path,props,count));p[0]^=1;
 CHECK(!zadt_recheck(&binding,path,props,count)); /* Failure poisons the binding; no automatic reopen. */
 CHECK(zadt_open(&binding,path,props,count));
}
int main(int argc,char**argv){
 if(argc!=2)return 2;FILE*f=fopen(argv[1],"rb");if(!f)return 2;CHECK(!fseek(f,0,SEEK_END));long n=ftell(f);CHECK(n>0&&n<1024*1024);rewind(f);tree=malloc((size_t)n);CHECK(tree!=NULL);CHECK(fread(tree,1,(size_t)n,f)==(size_t)n);fclose(f);tree_bytes=(size_t)n;
 SecureDTInit(tree,tree_bytes);
 exercise("/arm-io/ziyi-gpu-probe",zg_adt_properties,ZG_ADT_PROPERTY_COUNT);
 exercise("/arm-io/ziyi-wifi-probe",zw_adt_properties,ZW_ADT_PROPERTY_COUNT);
 DTEntry entry;CHECK(SecureDTLookupEntry(NULL,"/arm-io/ziyi-gpu-probe",&entry)==kSuccess);struct zg_contract gpu;CHECK(zg_contract_read(&gpu,actual_gpu_get,(void*)entry));
 CHECK(SecureDTLookupEntry(NULL,"/arm-io/ziyi-wifi-probe",&entry)==kSuccess);const void*v;unsigned bytes;CHECK(SecureDTGetProperty(entry,"contract",&v,&bytes)==kSuccess);CHECK(bytes==160);struct zwifi_contract wifi;memcpy(&wifi,v,160);CHECK(zwifi_contract_valid(&wifi));
 printf("ACTUAL_8796_SECURE_DT_API_PASS checks=%u SWEET=%u available=%u realSuccess=%d observed=%u noFakeLockdown=1\n",tests,SWEET,ZADT_LOCKDOWN_AVAILABLE,kSuccess,SecureDTIsLockedDown()?1:0);free(tree);return 0;
}
