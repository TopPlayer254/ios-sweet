/* Compiled twice with byte-exact fixed/predecessor helper and real XNU masks. */
#define main legacy_main_unused
#include "test_pmap_helper.c"
#undef main
int main(void){
 void*storage=calloc(1,4*16384);CHECK(storage!=NULL);tables=(tt_entry_t(*)[2048])(((uintptr_t)storage+16383u)&~(uintptr_t)16383u);
 CHECK(ARM_PTE_TYPE_MASK==2&&ARM_PTE_TYPE_VALID==3&&ARM_TTE_VALID==1);
 init();uint64_t aperture=0;
#ifdef G21_PREDECESSOR
 CHECK(!ziyi_hop_pmap_page(VA,PA,2,2,false,&aperture));CHECK(hop_last_error==3&&!writes&&!cas);
 struct g20_pmap_trace t;CHECK(ziyi_hop_pmap_trace(&t)&&t.reason==8&&t.value[G20_OWNED_TYPE]==2);
 CHECK((t.value[G20_OWNED_PTE]&3)==3&&(t.value[G20_PHYSICAL_PTE]&3)==3);
 printf("G21_PREDECESSOR_NATIVE_MASK_FAILURE_REPRODUCED rawValid3 masked2 compared3 stores0\n");
#else
 CHECK(ziyi_hop_pmap_page(VA,PA,2,2,false,&aperture)&&aperture==AP&&!writes&&!cas);
 CHECK(ziyi_hop_pmap_page(VA,PA,2,6,true,&aperture));CHECK(writes==1&&updates==1);
 for(unsigned side=0;side<2;side++)for(unsigned type=0;type<3;type++){
  init();tables[2][1+side]=(tables[2][1+side]&~3ULL)|type;aperture=0;
  CHECK(!ziyi_hop_pmap_page(VA,PA,2,6,true,&aperture));CHECK(hop_last_error==3&&!writes&&!flushes&&!updates&&!cas);
 }
 /* Valid original alias still refuses; a mapped different PA is not an alias. */
 init();tables[2][3]=PA|3;CHECK(!ziyi_hop_pmap_page(VA,PA,2,6,true,&aperture)&&hop_last_error==4&&!writes);
 init();tables[2][3]=(PA+PAGE_SIZE)|3;CHECK(ziyi_hop_pmap_page(VA,PA,2,2,false,&aperture)&&!writes);
 printf("G21_REAL_MASK_VALID_BIT_HOST_PASS checks=%u valid3=1 invalid012Rejected=1 extraAliasStillRefused=1 native=0\n",checks);
#endif
 free(storage);return 0;
}
