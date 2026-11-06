/* Execute the extracted ACTUAL XNU mapper, not a replacement algorithm.
 * Model only page-table allocation/address primitives. No device operations. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SWEET 1
#define XNU_MONITOR 0
#define __ARM_KERNEL_PROTECT__ 0
#define DEVELOPMENT 1
#ifndef __unused
#define __unused __attribute__((unused))
#endif
#define __improbable(x) (x)
#define SECURITY_READ_ONLY_LATE(x) x
#define FALSE 0
#define TRUE 1
#define PAGE_SHIFT 14
#define PAGE_SIZE 16384ULL
#define ARM_PGBYTES PAGE_SIZE
#define ARM_PGMASK (PAGE_SIZE-1)
#define ARM_TT_L2_SIZE (1ULL<<25)
#define ARM_TT_L2_OFFMASK (ARM_TT_L2_SIZE-1)
#define ARM_TT_TWIG_OFFMASK ARM_TT_L2_OFFMASK
#define ARM_TT_L3_SHIFT 14
#define ARM_TT_L3_INDEX_MASK (ARM_TT_L2_OFFMASK&~ARM_PGMASK)
#define ARM_TTE_TABLE_MASK 0x0000ffffffffc000ULL
#define ARM_TTE_TYPE_MASK 2ULL
#define ARM_TTE_TYPE_TABLE 2ULL
#define ARM_TTE_VALID 1ULL
#define ARM_TTE_TYPE_BLOCK 0ULL
#define ARM_TTE_BLOCK_L2_MASK (ARM_TTE_TABLE_MASK&~ARM_TT_L2_OFFMASK)
#define ARM_TTE_BLOCK_AF (1ULL<<10)
#define ARM_TTE_BLOCK_NX (1ULL<<54)
#define ARM_TTE_BLOCK_PNX (1ULL<<53)
#define ARM_TTE_BLOCK_AP(x) ((uint64_t)(x)<<6)
#define ARM_TTE_BLOCK_SH(x) ((uint64_t)(x)<<8)
#define ARM_TTE_BLOCK_ATTRINDX(x) ((uint64_t)(x)<<2)
#define ARM_PTE_AF ARM_TTE_BLOCK_AF
#define ARM_PTE_NX ARM_TTE_BLOCK_NX
#define ARM_PTE_PNX ARM_TTE_BLOCK_PNX
#define ARM_PTE_SH(x) ARM_TTE_BLOCK_SH(x)
#define ARM_PTE_AP(x) ARM_TTE_BLOCK_AP(x)
#define ARM_PTE_ATTRINDX(x) ARM_TTE_BLOCK_ATTRINDX(x)
#define ARM_PTE_TYPE 3ULL
#define ARM_PTE_TYPE_FAULT 0ULL
#define ARM_PTE_HINT (1ULL<<52)
#define ARM_PTE_HINT_ADDR_SHIFT 21
#define SH_OUTER_MEMORY 2
#define CACHE_ATTRINDX_DEFAULT 0
#define CACHE_ATTRINDX_WRITEBACK 0
#define ARM64_GRANULE_ALLOW_BLOCK 1u
#define ARM64_GRANULE_ALLOW_HINT 2u
#define PTOV_TABLE_SIZE 8
#define AP_RWNA 0
#define ROUND_TWIG(x) (((x)+ARM_TT_L2_OFFMASK)&~ARM_TT_L2_OFFMASK)
#define round_page(x) (((x)+ARM_PGMASK)&~ARM_PGMASK)
#define round_up_pte_hint_address(x) (((x)+(1ULL<<21)-1)&~((1ULL<<21)-1))
#define pa_to_tte(x) ((x)&ARM_TTE_TABLE_MASK)
#define bzero(p,n) memset((p),0,(n))
#define os_atomic_store(p,v,o) (*(p)=(v))
typedef uint64_t tt_entry_t,pt_entry_t,pmap_paddr_t,vm_map_address_t;
typedef uint64_t vm_offset_t,vm_size_t,addr64_t;
typedef int boolean_t;
typedef struct {uint64_t pa,va,len;} ptov_table_entry;
static uint64_t gPhysBase=0x900000000ULL,gVirtBase=0x1000000000ULL;
static uint64_t real_avail_end,physmap_base,mem_size=0x80000000,real_phys_size=0x80000000;
static boolean_t kva_active;
static unsigned ptov_index,allocated,checks;
static int use_contiguous_hint=1;
static uint64_t l2[256],tables[96][2048];
static uint64_t phystokv(uint64_t pa){
 assert(pa>=0x10000000&&pa<0x10000000+96*PAGE_SIZE);
 return (uint64_t)(uintptr_t)tables[(pa-0x10000000)/PAGE_SIZE];
}
static uint64_t kvtophys(uint64_t va){
 assert(va>=(uint64_t)(uintptr_t)tables&&va<(uint64_t)(uintptr_t)(tables+96));
 return 0x10000000+(va-(uint64_t)(uintptr_t)tables);
}
static void *alloc_ptpage(bool initial){(void)initial;assert(allocated<96);return tables[allocated++];}
static tt_entry_t *arm_kva_to_tte(uint64_t va){assert(va>=gVirtBase);unsigned i=(unsigned)((va-gVirtBase)>>25);assert(i<256);return &l2[i];}
static void flush_mmu_tlb(void){}
#define panic(...) do {fprintf(stderr,__VA_ARGS__);abort();}while(0)
#if G19_TEST
#include "g19_boot_leaf.inc"
#include "g19-construction.inc"
#else
#include "g18-construction.inc"
#endif
static uint64_t lookup(uint64_t va,unsigned *leaf,uint64_t *descriptor){
 uint64_t e=*arm_kva_to_tte(va);assert(e&ARM_TTE_VALID);
 if((e&ARM_TTE_TYPE_MASK)==ARM_TTE_TYPE_TABLE){
  e=((uint64_t*)(uintptr_t)phystokv(e&ARM_TTE_TABLE_MASK))[(va>>14)&2047];
  assert((e&3)==3);*leaf=1;*descriptor=e;return (e&ARM_TTE_TABLE_MASK)|(va&ARM_PGMASK);
 }
 *leaf=0;*descriptor=e;return (e&ARM_TTE_BLOCK_L2_MASK)|(va&ARM_TT_L2_OFFMASK);
}
static void run_case(uint64_t bytes,uint64_t split,unsigned slide,int enabled){
 memset(l2,0,sizeof(l2));memset(tables,0,sizeof(tables));allocated=ptov_index=0;
 kva_active=0;real_avail_end=gPhysBase+bytes;physmap_base=gVirtBase+0x1234000+(uint64_t)slide*16384;
#if G19_TEST
 ziyi_hop_leaf_mode=enabled;
#else
 (void)enabled;
#endif
 ptov_table_entry map[PTOV_TABLE_SIZE]={0};
 arm_vm_physmap_slide(map,gVirtBase,split,AP_RWNA,3);
 arm_vm_physmap_slide(map,gVirtBase+split,bytes-split,AP_RWNA,3);
 assert(ptov_index==2&&map[0].pa==gPhysBase&&map[1].pa==gPhysBase+split);
 assert((map[0].va&ARM_TT_L2_OFFMASK)==(map[0].pa&ARM_TT_L2_OFFMASK));
 assert((map[1].va&ARM_TT_L2_OFFMASK)==(map[1].pa&ARM_TT_L2_OFFMASK));
 assert(map[1].va>=map[0].va+map[0].len);
 unsigned leaves=0,blocks=0;
 for(unsigned r=0;r<2;r++)for(uint64_t off=0;off<map[r].len;off+=PAGE_SIZE){
  unsigned leaf;uint64_t entry;uint64_t pa=lookup(map[r].va+off,&leaf,&entry);
  assert(pa==map[r].pa+off&&pa<real_avail_end);
  assert((entry&(ARM_PTE_AF|ARM_PTE_NX|ARM_PTE_PNX))==(ARM_PTE_AF|ARM_PTE_NX|ARM_PTE_PNX));
  if(G19_TEST&&enabled)assert(leaf&&!(entry&ARM_PTE_HINT));
  leaves+=leaf;blocks+=!leaf;checks++;
 }
 if(G19_TEST&&enabled){assert(blocks==0&&allocated<=68&&allocated*PAGE_SIZE<=1114112);}
 else if(bytes>=2*ARM_TT_L2_SIZE)assert(blocks);
 printf("construction mode%d enable%d bytes%llu split%llu leaves%u blocks%u tables%u\n",G19_TEST,enabled,(unsigned long long)bytes,(unsigned long long)split,leaves,blocks,allocated);
}
int main(void){
 assert(mem_size==0x80000000ULL&&real_phys_size==0x80000000ULL);
 run_case(0x80000000,0x104000,0,1);run_case(0x80000000,0x104000,7,1);
 run_case(0x80000000,0x2400000,31,1);run_case(0x4000000,0x4000,1,1);
 run_case(0x80000000,0x104000,7,0);
#if G19_TEST
 kva_active=1;ziyi_hop_leaf_mode=1;assert(ziyi_hop_boot_leaf_ready());
 ziyi_hop_leaf_mode=0;assert(!ziyi_hop_boot_leaf_ready());
#endif
 printf("G19_ACTUAL_BOOT_MAPPER_HOST_PASS checks=%u sourceBodies=4 postSwitchMutation=0\n",checks);return 0;
}
