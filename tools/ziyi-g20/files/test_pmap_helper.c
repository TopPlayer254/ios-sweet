/* Direct tests of the candidate include against a MODELED private pmap.
 * Actual full pmap.c ARM64 compilation is a separate mandatory audit step. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define XNU_MONITOR 0
#define __ARM_PTE_PHYSMAP__ 0
#define PAGE_SIZE 16384u
#define PAGE_MASK 16383u
#define PAGE_RATIO 1
#define PMAP_TT_L3_LEVEL 3u
#define ARM_TTE_TYPE_MASK 2u
#define ARM_TTE_VALID 1u
#define ARM_TTE_TYPE_TABLE 2u
#define ARM_TTE_TABLE_MASK 0x0000ffffffffc000ULL
#define ARM_PTE_TYPE_MASK 2u
#define ARM_PTE_TYPE_VALID 3u
#define ARM_PTE_HINT_MASK (1ULL<<52)
#define ARM_PTE_ATTRINDXMASK 0x1cu
#define ARM_PTE_SHMASK 0x300u
#define PVH_FLAG_IOMMU 1
#define PVH_TYPE_PTEP 1u
#define PP_ATTR_WIMG_MASK 0xffu
#define PP_ATTR_WIMG(a) (a)
#define VM_WIMG_DEFAULT 2u
#define VM_WIMG_WCOMB 6u
#define pte_to_pa(a) ((a)&0x0000ffffffffc000ULL)
#define atop(a) ((a)>>14)
typedef uint64_t pt_entry_t,tt_entry_t,vm_map_address_t,vm_offset_t;
typedef uint32_t ppnum_t;
typedef uint16_t pp_attr_t;
struct page_table_level_info {uint64_t offmask;};
static const struct page_table_level_info level_info[4]={{0},{0xfffffff},{0x1ffffff},{0x3fff}};
typedef struct {unsigned pta_root_level,pta_max_level;const struct page_table_level_info *pta_level_info;} pt_attr_t;
typedef struct {uint64_t min,max;tt_entry_t *tte;} pmap;
typedef pmap *pmap_t;
typedef struct {pt_entry_t *pte;unsigned type;} pv_entry_t;
static unsigned checks,locks,writes,flushes,syncs,cache_syncs,updates,cas;
static bool locked,valid=true,iommu,wrong_map;
static uint64_t va_override;
static pp_attr_t pp_attr_table[1];
static tt_entry_t (*tables)[2048];
static pmap map;
static pmap_t kernel_pmap=&map;
static pv_entry_t pv,*head=&pv;
static pt_attr_t attr={1,3,level_info};
static const uint64_t gPhysBase=0x10000,gVirtBase=0xc000;
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"helper line %u: %s\n",__LINE__,#x);exit(1);}}while(0)
#define PA 0x10000ULL
#define VA 0x4000ULL
#define AP 0x8000ULL
static const pt_attr_t *pmap_get_pt_attr(pmap_t p){CHECK(p==kernel_pmap);return &attr;}
static unsigned ttn_index(const pt_attr_t *a,uint64_t va,unsigned level){(void)a;return level==3?(unsigned)(va>>14):0;}
static uint64_t phystokv(uint64_t pa){return pa==PA?AP:pa;}
static bool pa_valid(uint64_t pa){return valid&&pa==PA;}
static unsigned pa_index(uint64_t pa){CHECK(pa==PA);return 0;}
static void pvh_lock(unsigned i){CHECK(i==0&&!locked);locked=true;locks++;}
static void pvh_unlock(unsigned i){CHECK(i==0&&locked);locked=false;}
static pv_entry_t **pai_to_pvh(unsigned i){CHECK(i==0&&locked);return &head;}
static bool pvh_test_type(pv_entry_t **h,unsigned type){CHECK(locked);return (*h)->type==type;}
static pt_entry_t *pvh_ptep(pv_entry_t **h){CHECK(locked);return (*h)->pte;}
static bool pvh_ptep_is_iommu(pt_entry_t *p){(void)p;return iommu;}
static pmap_t ptep_get_pmap(pt_entry_t *p){(void)p;return wrong_map?NULL:kernel_pmap;}
static uint64_t ptep_get_va(pt_entry_t *p){(void)p;return va_override?va_override:VA;}
static uint64_t wimg_to_pte(unsigned w,uint64_t pa){CHECK(pa==PA);return w==VM_WIMG_WCOMB?4u:0x200u;}
static bool OSCompareAndSwap16(pp_attr_t old,pp_attr_t value,pp_attr_t *p)
{CHECK(locked&&*p==old);cas++;if(cas==1){*p|=0x2000;return false;}*p=value;return true;}
static void write_pte_strong(pt_entry_t *p,pt_entry_t v){CHECK(locked&&p==&tables[2][2]);*p=v;writes++;}
static void flush_mmu_tlb_region_asid_async(uint64_t va,uint64_t n,pmap_t p,bool leaf)
{CHECK(locked&&va==AP&&n==PAGE_SIZE&&p==kernel_pmap&&leaf);flushes++;}
static bool pmap_update_cache_attributes_locked(ppnum_t pn,unsigned w,bool tlbi)
{CHECK(locked&&pn==atop(PA)&&tlbi);tables[2][1]=(tables[2][1]&~(ARM_PTE_ATTRINDXMASK|ARM_PTE_SHMASK))|wimg_to_pte(w,PA);updates++;return true;}
static void arm64_sync_tlb(bool rt){CHECK(locked&&!rt);syncs++;}
static void pmap_sync_wimg(ppnum_t pn,unsigned before,unsigned after)
{CHECK(!locked&&pn==atop(PA)&&before!=after);cache_syncs++;}
#include "pmap_owned_page.inc"
static void init(void)
{g20_latched=g20_sequence=g20_call=0;memset(&g20_slot,0,sizeof(g20_slot));memset(tables,0,3*16384);map=(pmap){VA,0x100000,tables[0]};attr=(pt_attr_t){1,3,level_info};tables[0][0]=(uint64_t)(uintptr_t)tables[1]|3;tables[1][0]=(uint64_t)(uintptr_t)tables[2]|3;tables[2][1]=PA|0x200|3;tables[2][2]=PA|0x200|3;pv=(pv_entry_t){&tables[2][1],PVH_TYPE_PTEP};pp_attr_table[0]=0x1000|VM_WIMG_DEFAULT;locks=writes=flushes=syncs=cache_syncs=updates=cas=0;locked=false;valid=true;iommu=wrong_map=false;va_override=0;}
static void refused(unsigned test)
{init();uint64_t aperture=0;
 switch(test){
 case 0:valid=false;break;
 case 1:pv.type=2;break; /* Unexpected PV list (even singleton) */
 case 2:pv.pte=&tables[2][2];break;
 case 3:iommu=true;break;
 case 4:wrong_map=true;break;
 case 5:va_override=VA+PAGE_SIZE;break;
 case 6:tables[0][0]=PA|1;break; /* inherited root block */
 case 7:tables[1][0]=PA|1;break; /* inherited twig block */
 case 8:tables[1][0]=0;break;
 case 9:tables[2][1]=0;break;
 case 10:tables[2][2]=0;break;
 case 11:tables[2][1]^=PAGE_SIZE;break;
 case 12:tables[2][2]^=PAGE_SIZE;break;
 case 13:tables[2][1]|=ARM_PTE_HINT_MASK;break;
 case 14:tables[2][2]|=ARM_PTE_HINT_MASK;break;
 case 15:pp_attr_table[0]=0x1000|VM_WIMG_WCOMB;break;
 case 16:tables[2][1]^=4;break;
 case 17:tables[2][2]^=4;break;
 case 18:map.max=VA;break;
 case 19:tables[2][3]=PA|0x200|3;break; /* untracked original BOOT alias */
 }
 pp_attr_t old=pp_attr_table[0];pt_entry_t owned=tables[2][1],physical=tables[2][2];
 CHECK(!ziyi_hop_pmap_page(VA,PA,2,6,true,&aperture));
 CHECK(!locked&&!aperture&&!writes&&!flushes&&!syncs&&!cache_syncs&&!updates&&!cas);
 CHECK(pp_attr_table[0]==old&&tables[2][1]==owned&&tables[2][2]==physical);}
int main(void)
{void *storage=calloc(1,4*16384);CHECK(storage!=NULL);tables=(tt_entry_t(*)[2048])(((uintptr_t)storage+16383u)&~(uintptr_t)16383u);init();uint64_t aperture=0;CHECK(ziyi_hop_pmap_abi()==HOP_PMAP_ABI);CHECK(ziyi_hop_pmap_page(VA,PA,2,2,false,&aperture)&&aperture==AP&&!writes&&!cas);CHECK(ziyi_hop_pmap_page(VA,PA,2,6,true,&aperture));CHECK(!locked&&writes==1&&flushes==1&&syncs==1&&updates==1&&cache_syncs==1&&cas==2);CHECK((pp_attr_table[0]&0x3000)==0x3000);CHECK((pp_attr_table[0]&PP_ATTR_WIMG_MASK)==6);CHECK((tables[2][1]&(ARM_PTE_ATTRINDXMASK|ARM_PTE_SHMASK))==4&&(tables[2][2]&(ARM_PTE_ATTRINDXMASK|ARM_PTE_SHMASK))==4);CHECK(ziyi_hop_pmap_page(VA,PA,6,2,true,&aperture));CHECK(writes==2&&flushes==2&&syncs==2&&updates==2&&cache_syncs==2&&(pp_attr_table[0]&PP_ATTR_WIMG_MASK)==2);
 for(unsigned t=0;t<20;t++)refused(t);
 init();CHECK(!ziyi_hop_pmap_page(VA+1,PA,2,6,true,&aperture));CHECK(!ziyi_hop_pmap_page(VA,PA+1,2,6,true,&aperture));CHECK(!ziyi_hop_pmap_page(VA,1ULL<<36,2,6,true,&aperture));CHECK(!ziyi_hop_pmap_page(VA,PA,2,0,true,&aperture));CHECK(!ziyi_hop_pmap_page(VA,PA,2,6,true,NULL));CHECK(!locks&&!writes&&!cas);
 printf("HFI_PMAP_HELPER_MODELED_PRIMITIVES_PASS checks=%u exactInclude=1 blockRefusal=1 pvSingleton=1 CASretry=1 explicitNonPPLphysmap=1 nativePmap=0\n",checks);free(storage);return 0;}
