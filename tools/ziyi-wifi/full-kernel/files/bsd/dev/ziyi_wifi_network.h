/* SPDX-License-Identifier: MIT */
#pragma once
#include <stddef.h>
#include <stdint.h>
#define ZNP_MAX 97u
/* Exact two-line format. No NUL, control bytes, extra records or truncation. */
static int znp_valid(const uint8_t *p,size_t n){
 if(!p||n<11||n>ZNP_MAX||p[n-1]!='\n')return 0;
 size_t split=n;
 for(size_t i=0;i<n-1;i++){
  if(p[i]=='\n'){if(split!=n)return 0;split=i;}
  else if(p[i]<0x20||p[i]>0x7e)return 0;
 }
 return split>=1&&split<=32&&n-split-2>=8&&n-split-2<=63;
}
