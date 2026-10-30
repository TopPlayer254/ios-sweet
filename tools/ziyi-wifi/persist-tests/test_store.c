/* SPDX-License-Identifier: MIT */
#define ZNP_HOST_TEST
#include "ziyi_wifi_store.inc"
#include <assert.h>
static const uint8_t sample[]="synthetic\npassword\n";
static void reset(void){
 memset(&f,0,sizeof(f));memset(znp_network,0,sizeof(znp_network));znp_state=0;znp_size=0;
 strcpy(f.mount.stat.f_fstypename,"hfs");strcpy(f.mount.stat.f_mntfromname,"/dev/ufsroot0");
 f.root.kind=2;f.parent.kind=2;f.file.kind=3;f.persist=1;
}
static void installed(void){f.exists=1;f.bytes=sizeof(sample)-1;memcpy(f.data,sample,f.bytes);}
int main(void){unsigned checks=0;
 reset();assert(!znp_load(sample,sizeof(sample)-1));checks++;
 assert(f.creates==1&&f.writes==1&&f.syncs==2&&f.reads==2&&f.rootputs==1&&znp_size==sizeof(sample)-1&&!memcmp(znp_network,sample,znp_size));checks++;
 reset();installed();assert(!znp_load(sample,sizeof(sample)-1)&&!f.creates&&!f.writes&&!f.syncs);checks++;
 reset();installed();f.persist=0;assert(!znp_load(NULL,0)&&!f.writes&&znp_size);checks++;
 reset();installed();f.data[0]='x';assert(znp_load(sample,sizeof(sample)-1)==EEXIST&&!f.writes&&f.data[0]=='x'&&!znp_size);checks++;
 for(unsigned fault=0;fault<16;fault++){
  reset();
  switch(fault){
   case 0:strcpy(f.mount.stat.f_fstypename,"apfs");break;
   case 1:strcpy(f.mount.stat.f_mntfromname,"/dev/md0");break;
   case 2:f.badparent=1;break;
   case 3:f.attr_unsupported=1;break;
   case 4:f.create_race=1;break;
   case 5:f.symlink=1;break;
   case 6:f.shortwrite=1;break;
   case 7:f.writefail=1;break;
   case 8:f.syncfail=1;break;
   case 9:f.syncfail=2;break;
   case 10:f.shortread=1;break;
   case 11:f.closefail=1;break;
   case 12:f.reopen_id=1;break;
   case 13:f.readcorrupt=1;break;
   case 14:f.mount.readonly=1;break;
   case 15:f.persist=0;break;
  }
  assert(znp_load(sample,sizeof(sample)-1));checks++;
  assert(!znp_size&&f.rootputs==1);checks++;
  if(fault<=5||fault>=14){assert(!f.writes);checks++;}
 }
 reset();installed();f.badfile=1;assert(znp_load(sample,sizeof(sample)-1)&&!f.writes);checks++;
 reset();uint64_t n=0;assert(znp_file(sample,sizeof(sample)-1,&n)&&n==sizeof(sample)-1);checks++;
 unsigned writes=f.writes;assert(znp_file(sample,sizeof(sample)-1,&n)&&f.writes==writes);checks++;
 printf("ZIYI_NETWORK_VFS_MODEL_PASS %u checks; native persistence unproven\n",checks);
 return 0;
}
