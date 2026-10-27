/* SPDX-License-Identifier: MIT
 * Pinned vendor smem.c selects local/global hosts before dereferencing headers.
 * W4 first retains BOTH bounded tables even when later metadata is rejected.
 */
static uint16_t zs_u16(const uint8_t*p){return p[0]|(uint16_t)p[1]<<8;}
static uint32_t zs_u32(const uint8_t*p){return zs_u16(p)|(uint32_t)zs_u16(p+2)<<16;}
static int zs_equal(const uint8_t*a,const uint8_t*b,unsigned n){for(unsigned i=0;i<n;i++)if(a[i]!=b[i])return 0;return 1;}
static int zs_read_it(zw_read rd,void*ctx,struct zw_smem_record*r,uint32_t off,void*to,uint32_t n){
 if(r->error)return 0;if(off>ZW_SMEM_BYTES||n>ZW_SMEM_BYTES-off){r->error=ZW_BOUNDS;return 0;}
 r->reads++;if(!rd(ctx,off,to,n)){r->error=ZW_IO;return 0;}return 1;
}
static int zw_smem_observe(zw_read rd,void*ctx,struct zw_smem_record*r){
 if(!rd||!r)return 0;*r=(struct zw_smem_record){0};const uint32_t toc=(uint32_t)ZW_SMEM_BYTES-4096;
 for(unsigned pass=0;pass<2;pass++){
  uint8_t version[4];if(!zs_read_it(rd,ctx,r,0x5c,version,4)||!zs_read_it(rd,ctx,r,toc,r->toc[pass],32))return 0;
  uint8_t*t=r->toc[pass];uint32_t count=zs_u32(t+8);
  if(zs_u32(version)>>16!=12||!zs_equal(t,(const uint8_t*)"$TOC",4)||zs_u32(t+4)!=1||!count||count>32){r->error=ZW_FORMAT;return 0;}
  if(pass&&count!=r->count){r->error=ZW_CHANGED;return 0;}r->count=count;
  for(unsigned i=0;i<count;i++)if(!zs_read_it(rd,ctx,r,toc+32+48*i,r->entry[pass][i],48))return 0;
 }
 r->error=(uint32_t)zwifi_table_validate(r);if(r->error)return 0;
 for(unsigned pass=0;pass<2;pass++)for(unsigned i=0;i<r->count;i++){
  uint8_t*e=r->entry[pass][i];if(zwifi_entry_kind(e)!=2)continue;
  uint32_t off=zs_u32(e),size=zs_u32(e+4);uint8_t*p=r->partition[pass][i];
  if(!zs_read_it(rd,ctx,r,off,p,32))return 0;
  uint32_t uncached=zs_u32(p+12),cached=zs_u32(p+16);
  if(!zs_equal(p,(const uint8_t*)"$PRT",4)||!zs_equal(e+12,p+4,4)||zs_u32(p+8)!=size||uncached<32||uncached>cached||cached>size){r->error=ZW_FORMAT;return 0;}
  if(zs_u16(e+12)==0xfffe)r->global_entry=i+1;else r->wpss_entry=i+1;
 }
 if(!zs_equal(&r->partition[0][0][0],&r->partition[1][0][0],32*32)){r->error=ZW_CHANGED;return 0;}
 return 1;
}
