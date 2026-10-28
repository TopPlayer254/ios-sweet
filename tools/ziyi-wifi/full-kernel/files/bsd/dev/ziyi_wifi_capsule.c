/* SPDX-License-Identifier: MIT
 * Private firmware transport only. No firmware/PAS/device operation here.
 */
#include "ziyi_wifi_capsule.h"
#include "ziyi_g7_sha256.h"
static uint32_t u32(const uint8_t*p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint64_t u64(const uint8_t*p){return u32(p)|(uint64_t)u32(p+4)<<32;}
static int equal(const void*a,const void*b,size_t n){const uint8_t*x=a,*y=b;unsigned d=0;for(size_t i=0;i<n;i++)d|=x[i]^y[i];return !d;}
static int zero(const uint8_t*p,size_t n){for(size_t i=0;i<n;i++)if(p[i])return 0;return 1;}
static int path_valid(const uint8_t*p,size_t n){
 size_t component=0;if(!n||n>=96||p[0]=='/'||p[n-1]=='/')return 0;
 for(size_t i=0;i<n;i++){unsigned c=p[i];if(c=='/'){if(!component)return 0;component=0;}else{if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))return 0;component++;}if(c=='.'&&((!i||p[i-1]=='/')&&(i+1==n||p[i+1]=='/'||(p[i+1]=='.'&&(i+2==n||p[i+2]=='/')))))return 0;}
 return component!=0;
}
static size_t name_len(const uint8_t*p){size_t n=0;while(n<96&&p[n])n++;return n;}
int zwf_validate(const uint8_t*p,size_t capacity,struct zwf_view*out){
 static const uint8_t magic[16]="ZIYI_WIFI_SLOT1";
 if(!p||!out||capacity!=ZWF_CAPACITY)return 0;*out=(struct zwf_view){0};
 uint8_t h[ZWF_HEADER],digest[32],header_hash[32];for(unsigned i=0;i<ZWF_HEADER;i++)h[i]=p[i];for(unsigned i=0;i<32;i++)header_hash[i]=h[120+i];
 if(!equal(h,magic,16)||u32(h+16)!=1||u32(h+20)!=ZWF_CAPACITY||u32(h+24)!=ZWF_HEADER||u32(h+28)!=ZWF_BOARD||u32(h+36)!=ZWF_ENTRY||u32(h+40)!=ZWF_HEADER||u32(h+52)!=1||!zero(h+152,104))return 0;
 uint32_t count=u32(h+32),data=u32(h+44),used=u32(h+48);
 if(!count||count>ZWF_MAX_FILES||data<ZWF_HEADER+count*ZWF_ENTRY||(data&16383)||used<data||used>ZWF_CAPACITY)return 0;
 for(unsigned i=120;i<152;i++)h[i]=0;g7_sha256(h,ZWF_HEADER,digest);if(!equal(digest,p+120,32))return 0;
 g7_sha256(p+ZWF_HEADER,count*ZWF_ENTRY,digest);if(!equal(digest,p+56,32))return 0;
 g7_sha256(p+data,used-data,digest);if(!equal(digest,p+88,32))return 0;
 if(!zero(p+ZWF_HEADER+count*ZWF_ENTRY,data-ZWF_HEADER-count*ZWF_ENTRY)||!zero(p+used,capacity-used))return 0;
 uint64_t prior_end=data;
 for(unsigned i=0;i<count;i++){
  const uint8_t*e=p+ZWF_HEADER+i*ZWF_ENTRY;size_t n=name_len(e);uint64_t off=u64(e+96),bytes=u64(e+104);
  if(!path_valid(e,n)||!zero(e+n,96-n)||!zero(e+144,16)||!bytes||(off&16383)||off<prior_end||off>used||bytes>used-off)return 0;
  if(!zero(p+prior_end,(size_t)(off-prior_end)))return 0;
  for(unsigned j=0;j<i;j++){const uint8_t*q=p+ZWF_HEADER+j*ZWF_ENTRY;size_t qn=name_len(q);if(n==qn&&equal(e,q,n))return 0;}
  g7_sha256(p+off,(size_t)bytes,digest);if(!equal(digest,e+112,32))return 0;prior_end=off+bytes;
 }
 if(prior_end!=used||!equal(p,h,120)||!equal(p+120,header_hash,32)||!equal(p+152,h+152,104))return 0;
 out->base=p;out->count=count;out->data=data;out->used=used;for(unsigned i=0;i<ZWF_HEADER;i++)out->header[i]=p[i];return 1;
}
const uint8_t *zwf_find(const struct zwf_view*v,const char*name,uint64_t*size){
 if(size)*size=0;if(!v||!v->base||!name||!size||!v->count||v->count>ZWF_MAX_FILES||!equal(v->base,v->header,ZWF_HEADER))return 0;
 size_t n=0;while(n<96&&name[n])n++;if(!path_valid((const uint8_t*)name,n))return 0;
 for(unsigned i=0;i<v->count;i++){
  const uint8_t*e=v->base+ZWF_HEADER+i*ZWF_ENTRY;if(name_len(e)!=n||!equal(e,name,n))continue;
  uint64_t off=u64(e+96),bytes=u64(e+104);if(off<v->data||off>v->used||!bytes||bytes>v->used-off)return 0;
  uint8_t h[32];g7_sha256(v->base+off,(size_t)bytes,h);if(!equal(h,e+112,32))return 0;*size=bytes;return v->base+off;
 }
 return 0;
}
