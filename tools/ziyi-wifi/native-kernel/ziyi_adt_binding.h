/* SPDX-License-Identifier: MIT
 * Exact-source normalized Apple ADT binding. This is not a fabricated KTRR proof.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <pexpert/device_tree.h>

#if defined(KERNEL_INTEGRITY_KTRR) || defined(KERNEL_INTEGRITY_CTRR)
#define ZADT_LOCKDOWN_AVAILABLE 1u
#else
#define ZADT_LOCKDOWN_AVAILABLE 0u
#endif
enum {ZADT_NONE=0,ZADT_APPLE_PROTECTED=1,ZADT_SWEET_BOARD_SNAPSHOT=2};
enum {ZADT_OK=0,ZADT_PLATFORM,ZADT_NOT_PROTECTED,ZADT_NODE,ZADT_PROPERTY,ZADT_CHANGED};
struct zadt_property {const char *name;const uint8_t *expected;unsigned bytes;};
struct zadt_binding {DTEntry entry;unsigned mode,available,observed,ready,status,property,passes;};
static int zadt_equal(const void*aa,const void*bb,unsigned n){const uint8_t*a=aa,*b=bb;for(unsigned i=0;i<n;i++)if(a[i]!=b[i])return 0;return 1;}
static int zadt_policy(struct zadt_binding*b){
 b->available=ZADT_LOCKDOWN_AVAILABLE;b->observed=SecureDTIsLockedDown()?1u:0u;
 if(b->available){if(!b->observed){b->status=ZADT_NOT_PROTECTED;return 0;}b->mode=ZADT_APPLE_PROTECTED;return 1;}
#if SWEET
 /* No meaningful KTRR/CTRR distinction exists on SWEET. Do not call it locked. */
 b->mode=ZADT_SWEET_BOARD_SNAPSHOT;return 1;
#else
 b->status=ZADT_PLATFORM;return 0;
#endif
}
static int zadt_pass(struct zadt_binding*b,const char*path,const struct zadt_property*p,unsigned count){
 DTEntry node=0;b->status=ZADT_OK;b->property=0;
 if(!zadt_policy(b))return 0;
 if(SecureDTLookupEntry(NULL,path,&node)!=kSuccess||!node){b->status=ZADT_NODE;return 0;}
 if(b->entry&&b->entry!=node){b->status=ZADT_CHANGED;return 0;}
 for(unsigned i=0;i<count;i++){
  const void*v=0;unsigned n=0;
  if(SecureDTGetProperty(node,p[i].name,&v,&n)!=kSuccess||!v||n!=p[i].bytes||!zadt_equal(v,p[i].expected,n)){
   b->status=ZADT_PROPERTY;b->property=i+1;return 0;
  }
 }
 b->entry=node;b->passes++;return 1;
}
static int zadt_open(struct zadt_binding*b,const char*path,const struct zadt_property*p,unsigned count){
 if(!b||!path||!p||!count)return 0;*b=(struct zadt_binding){0};
 if(!zadt_pass(b,path,p,count)||!zadt_pass(b,path,p,count))return 0;
 b->ready=1;return 1;
}
static int zadt_recheck(struct zadt_binding*b,const char*path,const struct zadt_property*p,unsigned count){
 if(!b||!b->ready||!b->entry)return 0;
 if(!zadt_pass(b,path,p,count)){b->ready=0;return 0;}
 return 1;
}
