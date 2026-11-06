#include "ziyi_g20_trace.h"
static uint32_t g20_crc(const uint8_t*p){uint32_t c=~0u;for(unsigned i=0;i<G20_TRACE_BYTES;i++){c^=i>=12&&i<16?0:p[i];for(unsigned j=0;j<8;j++)c=(c>>1)^(0xedb88320u&-(c&1));}return ~c;}
static void g20_put(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint32_t g20_get(const uint8_t*p){uint32_t v=0;for(unsigned i=0;i<4;i++)v|=(uint32_t)p[i]<<(8*i);return v;}
static int g20_valid(const struct g20_pmap_trace*t){
 if(!t||!t->call||t->guard>5||t->valid&~0xfffffu||t->reason&~255u)return 0;
 for(unsigned i=0;i<20;i++)if(!(t->valid&(1u<<i))&&t->value[i])return 0;
 if(t->guard!=3)return t->reason==0;
 if(!t->reason||(t->reason&(t->reason-1)))return 0;
 const uint64_t*v=t->value;uint32_t need=0;int failed=0;
 switch(t->reason){
 case G20_BAD_IOMMU:need=1u<<G20_IOMMU;failed=v[G20_IOMMU]==1;break;
 case G20_BAD_PMAP:need=(1u<<G20_OWNER_PMAP)|(1u<<G20_KERNEL_PMAP);failed=v[G20_OWNER_PMAP]!=v[G20_KERNEL_PMAP];break;
 case G20_BAD_VA:need=(1u<<G20_OWNER_VA)|(1u<<G20_VA);failed=v[G20_OWNER_VA]!=v[G20_VA];break;
 case G20_BAD_OWNED_TYPE:need=1u<<G20_OWNED_TYPE;failed=v[G20_OWNED_TYPE]!=3;break;
 case G20_BAD_PHYSICAL_TYPE:need=1u<<G20_PHYSICAL_TYPE;failed=v[G20_PHYSICAL_TYPE]!=3;break;
 case G20_BAD_OWNED_PA:need=(1u<<G20_OWNED_PA)|(1u<<G20_PA);failed=v[G20_OWNED_PA]!=v[G20_PA];break;
 case G20_BAD_PHYSICAL_PA:need=(1u<<G20_PHYSICAL_PA)|(1u<<G20_PA);failed=v[G20_PHYSICAL_PA]!=v[G20_PA];break;
 case G20_BAD_HINT:need=(1u<<G20_OWNED_PTE)|(1u<<G20_PHYSICAL_PTE)|(1u<<G20_HINT_MASK);failed=((v[G20_OWNED_PTE]|v[G20_PHYSICAL_PTE])&v[G20_HINT_MASK])!=0;break;
 }
 return (t->valid&need)==need&&failed;
}
int g20_trace_encode(const struct g20_pmap_trace*t,uint8_t*out){
 if(!out||!g20_valid(t))return 0;uint32_t w[48]={0x31465047u,1,G20_TRACE_BYTES,0,t->call,t->guard,t->valid,t->reason};
 for(unsigned i=0;i<20;i++){w[8+2*i]=(uint32_t)t->value[i];w[9+2*i]=(uint32_t)(t->value[i]>>32);}
 for(unsigned i=0;i<48;i++)g20_put(out+4*i,w[i]);g20_put(out+12,g20_crc(out));return 1;
}
int g20_trace_decode(const uint8_t*p,size_t n,struct g20_pmap_trace*t){
 if(!p||!t||n!=G20_TRACE_BYTES||g20_get(p)!=0x31465047u||g20_get(p+4)!=1||g20_get(p+8)!=n||g20_get(p+12)!=g20_crc(p))return 0;
 struct g20_pmap_trace r={g20_get(p+16),g20_get(p+20),g20_get(p+24),g20_get(p+28),{0}};
 for(unsigned i=0;i<20;i++)r.value[i]=g20_get(p+32+8*i)|(uint64_t)g20_get(p+36+8*i)<<32;
 if(!g20_valid(&r))return 0;*t=r;return 1;
}
