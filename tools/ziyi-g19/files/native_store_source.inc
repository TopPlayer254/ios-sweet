static int zg_hfi_store(void*ctx,uint64_t address){
 struct zg_state*s=ctx;struct g19_hfi_health health={0};
 if((address!=G19_INIT&&address!=G19_BELL)||!zg_adt_check(s,0)||
    !zg_hfi_health(s,&health))return 0;
 if(address==G19_INIT){
  if(s->hfi_record.stage!=G19_HFI_INITIALIZE||s->hfi_record.start_calls!=1)return 0;
  int good;uint32_t before=(uint32_t)zg_g8_load(s,G19_INIT,4,&good);
  if(!good||before>1)return 0;
 }else{
  if(s->hfi_record.stage!=G19_HFI_PUBLISH||s->hfi_record.doorbells!=1||
     s->hfi_record.q0_write_stores!=1||s->hfi_record.stores!=5)return 0;
  volatile uint32_t*q=(volatile uint32_t*)(uintptr_t)s->hfi_memory.va;
  if(q[68/4]!=4||q[GN_HFI_QUEUE0/4]!=0x00100201||
     q[GN_HFI_QUEUE0/4+1]!=GH_A662_SUPPORTED_VERSION)return 0;
 }
 /* Exact finite source-qualified stores. Doorbell is write-only: no
  * fictitious readback or general bit mask is used as authorization. */
 void*p=zg_g8_pointer(s,address,4,1);if(!p)return 0;
 zg_barrier(s);*(volatile uint32_t*)p=1;zg_barrier(s);return 1;
}
