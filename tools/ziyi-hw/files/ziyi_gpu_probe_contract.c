/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_gpu_probe_contract.h"
#include "ziyi_gpu_probe_config.h"
static int equal(const void*a,const void*b,size_t n){const unsigned char*x=a,*y=b;for(size_t i=0;i<n;i++)if(x[i]!=y[i])return 0;return 1;}
static uint32_t le32(const unsigned char*p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint64_t le64(const unsigned char*p){return le32(p)|(uint64_t)le32(p+4)<<32;}
static int exact(zg_get_property get,void*ctx,const char*name,const void*want,unsigned bytes){const void*v=0;unsigned n=0;return get(ctx,name,&v,&n)&&v&&n==bytes&&equal(v,want,bytes);}
static int number(zg_get_property get,void*ctx,const char*name,uint32_t want){const void*v=0;unsigned n=0;return get(ctx,name,&v,&n)&&v&&n==4&&le32(v)==want;}
int zg_contract_read(struct zg_contract*out,zg_get_property get,void*ctx){
 if(!out||!get)return 0;*out=(struct zg_contract){0};
 static const char compatible[]="ziyi,gpu-probe-v1",names[]="gcc\0gpucc\0gmu\0smmu";
 if(!exact(get,ctx,"compatible",compatible,sizeof(compatible))||!exact(get,ctx,"reg-names",names,sizeof(names))||
 !number(get,ctx,"source-board-id",0x06060201)||!number(get,ctx,"hlos-context-banks",6)||!number(get,ctx,"hlos-smr",6)||
 !number(get,ctx,"gmu-sid",5)||!number(get,ctx,"gmu-sid-mask",0x400)||!number(get,ctx,"source-handoff-count",0)||!number(get,ctx,"source-gmu-reg-size",0x34000)||
 !exact(get,ctx,"source-contract-sha256",zg_source_digest,sizeof(zg_source_digest)))return 0;
 const void*v=0;unsigned n=0;if(!get(ctx,"reg",&v,&n)||!v||n!=64)return 0;
 const struct zg_region expected[4]={{0x100000,0x90000},{0x3d90000,0xa000},{0x3d6a000,0x25000},{0x3da0000,0x20000}};
 for(unsigned i=0;i<4;i++){uint64_t a=le64((const unsigned char*)v+16*i),b=le64((const unsigned char*)v+16*i+8);if(a!=expected[i].base||b!=expected[i].bytes)return 0;out->regions[i]=expected[i];}
 out->board=0x06060201;out->cb=out->smr=6;out->sid=5;out->sid_mask=0x400;return 1;
}
int zg_kernel_context(uint64_t el,uint64_t sctlr,uint64_t hz){return el==4&&(sctlr&0x1005)==0x1005&&!(sctlr&((1ULL<<24)|(1ULL<<25)))&&hz==19200000;}
int zg_map_geometry(uint64_t base,uint64_t bytes,uint64_t page,struct zg_geometry*out){
 if(!out)return 0;*out=(struct zg_geometry){0};if(!bytes||!page||page<4096||page>65536||(page&(page-1))||base>UINT64_MAX-bytes)return 0;
 uint64_t low=base&~(page-1),bias=base-low;if(bytes>UINT64_MAX-bias-(page-1))return 0;
 uint64_t span=(bytes+bias+page-1)&~(page-1);if(!span||low>UINT64_MAX-span)return 0;
 *out=(struct zg_geometry){low,span,bias};return 1;
}
int zg_region_offset(const struct zg_region*r,uint64_t at,uint64_t n,uint64_t*off){if(!r||!n||!off||at<r->base||at-r->base>=r->bytes||n>r->bytes-(at-r->base))return 0;*off=at-r->base;return 1;}
