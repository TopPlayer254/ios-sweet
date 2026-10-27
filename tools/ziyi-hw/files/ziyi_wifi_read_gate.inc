_Static_assert(ZW_SMEM_BASE==0x80900000ULL&&ZW_SMEM_BYTES==0x200000ULL,"source-bound SMEM window");
static const struct zwifi_fixed_read zwifi_fixed_reads[2]={{ZW_SMEM_BASE+0x5c,4,0},{ZW_SMEM_BASE+ZW_SMEM_BYTES-4096,32,0}};
static uint16_t zr_u16(const uint8_t*p){return p[0]|(uint16_t)p[1]<<8;}
static uint32_t zr_u32(const uint8_t*p){return zr_u16(p)|(uint32_t)zr_u16(p+2)<<16;}
static int zr_equal(const uint8_t*a,const uint8_t*b,unsigned n){for(unsigned i=0;i<n;i++)if(a[i]!=b[i])return 0;return 1;}
/* 0 empty, 1 excluded metadata, 2 selected header. Vendor host filtering
 * precedes header access. Excluded entries never authorize a heap read. */
static int zwifi_entry_kind(const uint8_t*e){
 uint32_t off=zr_u32(e),size=zr_u32(e+4);unsigned h0=zr_u16(e+12),h1=zr_u16(e+14);
 if(!off&&!size)return 0;
 return ((h0==0&&h1==13)||(h0==13&&h1==0)||(h0==0xfffe&&h1==0xfffe))?2:1;
}
static int zwifi_table_validate(const struct zw_smem_record*r){
 if(!r||!r->count||r->count>32)return ZW_FORMAT;
 if(!zr_equal(r->toc[0],r->toc[1],32)||!zr_equal(&r->entry[0][0][0],&r->entry[1][0][0],32*48))return ZW_CHANGED;
 const uint8_t*t=r->toc[0];if(!zr_equal(t,(const uint8_t*)"$TOC",4)||zr_u32(t+4)!=1||zr_u32(t+8)!=r->count)return ZW_FORMAT;
 unsigned wp=0,global=0;const uint32_t toc=(uint32_t)ZW_SMEM_BYTES-4096;
 for(unsigned i=0;i<r->count;i++){
  const uint8_t*e=r->entry[0][i];if(zwifi_entry_kind(e)!=2)continue;
  uint32_t off=zr_u32(e),size=zr_u32(e+4),line=zr_u32(e+16);
  /* First4KiB legacy header and final4KiB TOC are protected, not partitions. */
  if(off<4096||off>toc||size<32||size>toc-off||((off|size)&7)||(line&&(line>4096||line&(line-1))))return ZW_BOUNDS;
  /* Reject selected overlap with ANY retained nonempty span; excluded spans
   * use widened arithmetic and cannot become authority through validation. */
  for(unsigned j=0;j<r->count;j++)if(j!=i){
   const uint8_t*other=r->entry[0][j];uint64_t o=zr_u32(other),n=zr_u32(other+4);
   if(n&&(uint64_t)off<o+n&&o<(uint64_t)off+size)return ZW_OVERLAP;
  }
  if(zr_u16(e+12)==0xfffe){if(++global>1)return ZW_FORMAT;}else if(++wp>1)return ZW_FORMAT;
 }
 return ZW_OK;
}
static int zwifi_address_allowed(const struct zwifi_record*r,uint64_t physical,uint32_t bytes){
 if(!r||r->stage!=3||r->status||!zwifi_context_valid(r)||(physical&3)||(bytes&3)||physical<ZW_SMEM_BASE||physical>ZW_SMEM_BASE+ZW_SMEM_BYTES||bytes>ZW_SMEM_BASE+ZW_SMEM_BYTES-physical)return 0;
 for(unsigned i=0;i<2;i++)if(physical==zwifi_fixed_reads[i].physical&&bytes==zwifi_fixed_reads[i].bytes)return 1;
 uint32_t count=r->smem.count,off=(uint32_t)(physical-ZW_SMEM_BASE),toc=(uint32_t)ZW_SMEM_BYTES-4096;
 if(!count||count>32)return 0;
 if(bytes==48&&off>=toc+32&&off<toc+32+48*count&&(off-(toc+32))%48==0)return 1;
 if(bytes!=32||zwifi_table_validate(&r->smem))return 0;
 for(unsigned i=0;i<count;i++)if(zwifi_entry_kind(r->smem.entry[0][i])==2&&zr_u32(r->smem.entry[0][i])==off)return 1;
 return 0;
}
