/* SPDX-License-Identifier: GPL-2.0-only
 * Names mirror Xiaomi 24856283 icnss2/qmi.c:icnss_get_bdf_file_name().
 */
#include "bdf.h"
int zwn_bdf_elf(uint32_t board,int l9s,int global,char*out,size_t capacity)
{
 if(!out||capacity<32||(l9s!=0&&l9s!=1)||(global!=0&&global!=1))return 0;
 char name[32]="qca6750/";unsigned at=8;const char*tail;
 const char*hex="0123456789abcdef";
 if(board==0xff){
  tail=l9s?(global?"bd_l9sgl.elf":"bd_l9s.elf"):"bdwlan.elf";
  for(unsigned i=0;tail[i];i++)name[at++]=tail[i];
 }else{
  tail="bdwlan";for(unsigned i=0;tail[i];i++)name[at++]=tail[i];
  if(board<0xff){name[at++]='.';name[at++]='e';name[at++]=hex[board>>4];name[at++]=hex[board&15];}
  else{
   name[at++]=hex[(board>>12)&15];name[at++]=hex[(board>>8)&15];
   name[at++]='.';name[at++]='e';name[at++]=hex[(board>>4)&15];name[at++]=hex[board&15];
  }
 }
 name[at]=0;for(unsigned i=0;i<=at;i++)out[i]=name[i];return 1;
}
