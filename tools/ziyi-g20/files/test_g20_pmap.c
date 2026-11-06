#define main g19_legacy_main
#include "test_pmap_helper.c"
#undef main
#include "ziyi_g20_trace.c"
int main(int argc,char**argv){
 CHECK(argc==2);CHECK(g19_legacy_main()==0);
 void*storage=calloc(1,4*16384);CHECK(storage!=NULL);tables=(tt_entry_t(*)[2048])(((uintptr_t)storage+16383u)&~(uintptr_t)16383u);
 const unsigned faults[]={3,4,5,9,10,11,12,13,14};
 const unsigned reasons[]={1,2,4,8,16,32,64,128,128};
 for(unsigned i=0;i<9;i++){
  refused(faults[i]);struct g20_pmap_trace t={0},copy={0};CHECK(ziyi_hop_pmap_trace(&t));
  CHECK(t.call==1&&t.guard==3&&t.reason==reasons[i]);CHECK(t.value[G20_VA]==VA&&t.value[G20_PA]==PA);
  uint8_t raw[192],twin[192];CHECK(g20_trace_encode(&t,raw));CHECK(g20_trace_decode(raw,192,&copy));CHECK(!memcmp(&copy,&t,sizeof(t)));
  char path[1024];int n=snprintf(path,sizeof(path),"%s/reason-%u-%u.bin",argv[1],t.reason,i);CHECK(n>0&&(unsigned)n<sizeof(path));FILE*f=fopen(path,"wb");CHECK(f!=NULL);CHECK(fwrite(raw,1,192,f)==192);CHECK(!fclose(f));
  for(unsigned bit=0;bit<1536;bit++){memcpy(twin,raw,192);twin[bit/8]^=1u<<(bit%8);CHECK(!g20_trace_decode(twin,192,&copy));}
  /* Later failures must not overwrite the first refusal snapshot. */
  uint64_t aperture=0;CHECK(!ziyi_hop_pmap_page(VA+1,PA,2,6,true,&aperture));CHECK(ziyi_hop_pmap_trace(&copy));CHECK(!memcmp(&copy,&t,sizeof(t)));
  CHECK(!writes&&!flushes&&!updates&&!cas);CHECK(!ziyi_hop_pmap_trace(NULL));
  uint32_t seq=g20_sequence;g20_sequence|=1;CHECK(!ziyi_hop_pmap_trace(&copy));g20_sequence=seq;
 }
 init();uint64_t aperture=0;CHECK(ziyi_hop_pmap_page(VA,PA,2,2,false,&aperture));struct g20_pmap_trace t;CHECK(ziyi_hop_pmap_trace(&t)&&!t.guard&&!t.reason);uint8_t raw[192];CHECK(g20_trace_encode(&t,raw));t.reason=3;CHECK(!g20_trace_encode(&t,raw));
 printf("G20_PMAP_CPU_TRACE_HOST_PASS checks=%u guard3Cases=9 corruptBits=13824 noPermissionChange=1 noMMIO=1 native=0\n",checks);free(storage);return 0;
}
