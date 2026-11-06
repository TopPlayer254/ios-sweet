/* SPDX-License-Identifier: GPL-2.0-only */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ziyi_g19_owned_pages.h"
#define VA 0x100000000ULL
#define AP 0x200000000ULL
#define PA 0x80000000ULL
#define EPOCH 71ULL
static unsigned checks;
#define CHECK(x) do { checks++;if(!(x)){fprintf(stderr,"line %u: %s\n",__LINE__,#x);exit(1);} }while(0)
enum fault { NONE, BAD_POLICY, BAD_GRANULE, BAD_VA, HIGH_PA, UNMANAGED,
 DUP_PA, BAD_APERTURE, EXTRA_ALIAS, DEVICE_ATTR, ALIAS_CACHED,
 TRANSLATION_FAULT, WRONG_PA, WRONG_VA, WRONG_EL, MMU_OFF, WRONG_EXPECTED,
 STALE_EPOCH, SET_NO_EFFECT, SET_PARTIAL_FAIL };
struct model {
 unsigned calls,fail_at,allocs,frees,stores,observations,live_facts;
 unsigned n;enum fault fault;
 uint32_t wimg[HOP_MAX_PAGES];uint8_t attr[HOP_MAX_PAGES];
};
static int step(struct model *m){return ++m->calls!=m->fail_at;}
static unsigned index_pa(uint64_t pa){return (unsigned)((pa-PA)/32768u);}
static int policy(void *v,uint32_t *g,uint32_t *nc)
{struct model *m=v;if(!step(m))return 0;*g=m->fault==BAD_GRANULE?4096:HOP_PAGE_SIZE;*nc=6;return m->fault!=BAD_POLICY;}
static int evidence(void *v,uint64_t e,uint32_t *f)
{struct model *m=v;if(!step(m))return 0;*f=m->live_facts;return e==EPOCH&&m->fault!=STALE_EPOCH;}
static int allocate(void *v,uint64_t e,uint32_t b,uint64_t *va)
{struct model *m=v;if(!step(m))return 0;CHECK(e==EPOCH&&b==m->n*HOP_PAGE_SIZE);m->allocs++;*va=VA+(m->fault==BAD_VA?4096u:0);return 1;}
static int backing(void *v,uint64_t va,uint64_t *pa,uint64_t *ap)
{struct model *m=v;if(!step(m))return 0;unsigned i=(unsigned)((va-VA)/HOP_PAGE_SIZE);CHECK(i<m->n);*pa=PA+i*32768u;*ap=AP+i*HOP_PAGE_SIZE;if(m->fault==DUP_PA&&i)*pa=PA;if(m->fault==HIGH_PA)*pa=1ULL<<36;if(m->fault==BAD_APERTURE)*ap+=4096;return 1;}
static int managed(void *v,uint64_t pa)
{struct model *m=v;(void)pa;return step(m)&&m->fault!=UNMANAGED;}
static int aliases(void *v,uint64_t e,uint64_t va,uint64_t pa)
{struct model *m=v;(void)va;(void)pa;return step(m)&&e==EPOCH&&m->fault!=EXTRA_ALIAS;}
static int get(void *v,uint64_t pa,uint32_t *a)
{struct model *m=v;if(!step(m))return 0;unsigned i=index_pa(pa);CHECK(i<m->n);*a=m->wimg[i];return 1;}
static int set(void *v,uint64_t pa,uint32_t a)
{struct model *m=v;if(!step(m))return 0;unsigned i=index_pa(pa);CHECK(i<m->n);m->stores++;if(m->fault!=SET_NO_EFFECT){m->wimg[i]=a;m->attr[i]=(uint8_t)(a==6?0x44:0xff);}return m->fault!=SET_PARTIAL_FAIL;}
static int observe(void *v,uint64_t va,uint64_t pa,struct gh_cpu_page_observation *o)
{struct model *m=v;if(!step(m))return 0;unsigned i=index_pa(pa);CHECK(i<m->n);uint8_t attr=m->attr[i];m->observations++;if(m->fault==DEVICE_ATTR)attr=0;if(m->fault==ALIAS_CACHED&&m->wimg[i]==6&&va>=AP)attr=0xff;
 *o=(struct gh_cpu_page_observation){va,pa,((uint64_t)attr<<56)|(pa&~4095ULL),4,1};
 if(m->fault==TRANSLATION_FAULT)o->par|=1;
 if(m->fault==WRONG_PA)o->par^=0x1000;
 if(m->fault==WRONG_VA)o->va^=0x1000;
 if(m->fault==WRONG_EXPECTED)o->expected_pa^=0x1000;
 if(m->fault==WRONG_EL)o->el=8;
 if(m->fault==MMU_OFF)o->sctlr=0;
 return 1;}
static int free_all(void *v,uint64_t e,uint64_t va,uint32_t b)
{struct model *m=v;if(!step(m))return 0;CHECK(e==EPOCH&&va==VA&&b==m->n*HOP_PAGE_SIZE);for(unsigned i=0;i<m->n;i++)CHECK(m->wimg[i]==2&&m->attr[i]==0xff);m->frees++;return 1;}
static struct hop_ops init(struct model *m,unsigned n)
{memset(m,0,sizeof(*m));m->n=n;m->live_facts=HOP_RECLAIM;for(unsigned i=0;i<n;i++){m->wimg[i]=2;m->attr[i]=0xff;}return(struct hop_ops){m,policy,evidence,allocate,backing,managed,aliases,get,set,observe,free_all};}
static void happy(void)
{for(unsigned n=1;n<=HOP_MAX_PAGES;n++){struct model m;struct hop h={0};struct hop_ops o=init(&m,n);CHECK(hop_prepare(&h,&o,EPOCH,n)&&h.state==HOP_READY&&m.allocs==1&&m.frees==0);CHECK(m.observations>=n*16u);CHECK(hop_mark_live(&h,&o)&&h.state==HOP_LIVE);m.live_facts=HOP_LEASE_HELD;CHECK(hop_validate(&h,&o));m.live_facts=HOP_RECLAIM;CHECK(hop_release(&h,&o)&&h.state==HOP_RELEASED&&m.frees==1);unsigned calls=m.calls;CHECK(!hop_release(&h,&o)&&!hop_prepare(&h,&o,EPOCH,n)&&m.calls==calls&&m.allocs==1&&m.frees==1);}}
static void invalid(void)
{struct model m;struct hop h={0};struct hop_ops o=init(&m,2);CHECK(!hop_prepare(NULL,&o,EPOCH,1));CHECK(!hop_prepare(&h,&o,0,2)&&!m.allocs);h=(struct hop){0};CHECK(!hop_prepare(&h,&o,EPOCH,HOP_MAX_PAGES+1)&&!m.allocs);h=(struct hop){0};CHECK(!hop_prepare(&h,&o,EPOCH,0)&&!m.allocs);
 for(enum fault f=BAD_POLICY;f<=SET_PARTIAL_FAIL;f++){o=init(&m,2);h=(struct hop){0};m.fault=f;CHECK(!hop_prepare(&h,&o,EPOCH,2));CHECK(!m.frees);CHECK(m.allocs?h.state==HOP_QUARANTINE:h.state==HOP_REFUSED);unsigned calls=m.calls;CHECK(!hop_mark_live(&h,&o)&&!hop_release(&h,&o)&&!hop_validate(&h,&o)&&!hop_prepare(&h,&o,EPOCH,2)&&m.calls==calls);if(f==EXTRA_ALIAS||f==DUP_PA||f==HIGH_PA)CHECK(m.stores==0);if(f==SET_PARTIAL_FAIL)CHECK(m.stores==1&&m.attr[0]==0x44&&h.page[0].touched);}}
static void fail_every_callback(void)
{struct model m;struct hop h={0};struct hop_ops o=init(&m,2);CHECK(hop_prepare(&h,&o,EPOCH,2));unsigned prepare_calls=m.calls;
 for(unsigned fail_at=1;fail_at<=prepare_calls;fail_at++){o=init(&m,2);h=(struct hop){0};m.fail_at=fail_at;CHECK(!hop_prepare(&h,&o,EPOCH,2));CHECK(!m.frees);CHECK(m.allocs?h.state==HOP_QUARANTINE:h.state==HOP_REFUSED);unsigned calls=m.calls;CHECK(!hop_release(&h,&o)&&!hop_prepare(&h,&o,EPOCH,2)&&m.calls==calls);}
 o=init(&m,2);h=(struct hop){0};CHECK(hop_prepare(&h,&o,EPOCH,2));m.calls=0;CHECK(hop_mark_live(&h,&o));unsigned publish_calls=m.calls;
 for(unsigned f=1;f<=publish_calls;f++){o=init(&m,2);h=(struct hop){0};CHECK(hop_prepare(&h,&o,EPOCH,2));m.calls=0;m.fail_at=f;CHECK(!hop_mark_live(&h,&o)&&h.state==HOP_QUARANTINE&&!m.frees);}
 o=init(&m,2);h=(struct hop){0};CHECK(hop_prepare(&h,&o,EPOCH,2));m.calls=0;CHECK(hop_release(&h,&o));unsigned release_calls=m.calls;
 for(unsigned f=1;f<=release_calls;f++){o=init(&m,2);h=(struct hop){0};CHECK(hop_prepare(&h,&o,EPOCH,2));m.calls=0;m.fail_at=f;CHECK(!hop_release(&h,&o)&&h.state==HOP_QUARANTINE&&!m.frees);unsigned calls=m.calls;CHECK(!hop_release(&h,&o)&&m.calls==calls);}
 printf("fault matrix: prepare=%u mark_live=%u release=%u callback positions\n",prepare_calls,publish_calls,release_calls);}
static void lifecycle(void)
{for(unsigned bit=HOP_LEASE_HELD;bit<=HOP_GMU_UNMAPPED;bit<<=1){struct model m;struct hop h={0};struct hop_ops o=init(&m,2);CHECK(hop_prepare(&h,&o,EPOCH,2));CHECK(hop_mark_live(&h,&o));unsigned stores=m.stores;m.live_facts=HOP_RECLAIM&~bit;CHECK(!hop_release(&h,&o)&&h.state==HOP_QUARANTINE&&!m.frees&&m.stores==stores);}
 for(enum fault f=EXTRA_ALIAS;f<=MMU_OFF;f++){if(f==DEVICE_ATTR)continue;struct model m;struct hop h={0};struct hop_ops o=init(&m,2);CHECK(hop_prepare(&h,&o,EPOCH,2));m.fault=f;CHECK(!hop_validate(&h,&o)&&h.state==HOP_QUARANTINE&&!m.frees);}}
int main(void)
{happy();invalid();fail_every_callback();lifecycle();printf("HFI_OWNED_PAGES_HOST_PASS checks=%u nativeAllocation=0 nativeAT=0 nativeLifecycle=0\n",checks);return 0;}
