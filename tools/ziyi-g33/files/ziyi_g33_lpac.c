#include "ziyi_g33_lpac.h"

uint64_t g33_address(enum g33_register id) {
 static const uint64_t addresses[G33_REGISTERS] = {
  0x03da0800,0x03da0c00,0x03da0804,0x03da0c04,0x03da0808,0x03da0c08
 };
 return (unsigned)id<G33_REGISTERS?addresses[id]:0;
}
static int g33_lpac_refuse(struct g33_record *r, unsigned error) {
 r->failed_stage=r->stage; __atomic_store_n(&r->error,error,__ATOMIC_RELEASE); __atomic_store_n(&r->stage,G33_REFUSED,__ATOMIC_RELEASE); return 0;
}
static int g33_lpac_guard(struct g33_record *r,const struct g33_io *io) {
 return io->guard(io->ctx,r->epoch,r->table_pa) || g33_lpac_refuse(r,G33_OWNER);
}
static int g33_lpac_read_word(struct g33_record *r,const struct g33_io *io,
 enum g33_register id,uint32_t *value) {
 if(!g33_lpac_guard(r,io))return 0;
 if(!io->read(io->ctx,id,value))return g33_lpac_refuse(r,G33_IO);
 r->reads++;
 return g33_lpac_guard(r,io);
}
static int g33_lpac_exact(struct g33_record *r,const struct g33_io *io,
 enum g33_register id,uint32_t expected) {
 uint32_t value=0;
 if(!g33_lpac_read_word(r,io,id,&value))return 0;
 r->after[id]=value;r->after_valid|=1u<<id;
 return value==expected || g33_lpac_refuse(r,G33_READBACK);
}
static int g33_lpac_issue(struct g33_record *r,const struct g33_io *io,
 enum g33_register id,uint32_t value,unsigned stage) {
 if(!((id==G33_LPAC_CONTROL&&value==0x300&&stage==G33_CONTROL_ISSUED)||
      (id==G33_LPAC_MATCH&&value==0x84000001&&stage==G33_MATCH_ISSUED)))
  return g33_lpac_refuse(r,G33_ARGUMENT);
 if(!g33_lpac_guard(r,io))return 0;
 /* No rollback after an issued transaction: a match may already admit DMA. */
 __atomic_store_n(&r->stage,stage,__ATOMIC_RELEASE);r->retained=1;r->writes++;
 if(!io->write(io->ctx,id,value)||!io->order(io->ctx))return g33_lpac_refuse(r,G33_IO);
 return g33_lpac_exact(r,io,id,value);
}
int g33_install(struct g33_record *r,const struct g33_io *io,uint64_t epoch,
 uint64_t core_table_pa) {
 static const uint32_t empty[G33_REGISTERS]={0x84000005,0x302,0x84000000,0x300,0,0x20000};
 static const uint32_t live[G33_REGISTERS]={0x84000005,0x302,0x84000000,0x300,0x84000001,0x300};
 if(!r||!io||!io->guard||!io->read||!io->write||!io->order||
    !epoch||!core_table_pa||(core_table_pa&16383)||core_table_pa>=(1ull<<36))return 0;
 if(r->stage!=G33_NEW||r->epoch||r->table_pa||r->before_valid||r->after_valid||r->retained||r->reads||r->writes)return 0;
 r->epoch=epoch;r->table_pa=core_table_pa;__atomic_store_n(&r->stage,G33_SNAPSHOT,__ATOMIC_RELEASE);
 for(unsigned pass=0;pass<2;pass++)for(unsigned i=0;i<G33_REGISTERS;i++) {
  uint32_t value=0;if(!g33_lpac_read_word(r,io,(enum g33_register)i,&value))return 0;
  if(!pass){r->before[i]=value;r->before_valid|=1u<<i;}
  if(value!=empty[i])return g33_lpac_refuse(r,G33_OCCUPIED);
 }
 /* Bind only SID 1 / 0x401 to the already owned core context. SID 2 and 5
  * remain outside this match. No combined mask or context-bank write. */
 if(!g33_lpac_issue(r,io,G33_LPAC_CONTROL,0x300,G33_CONTROL_ISSUED))return 0;
 __atomic_store_n(&r->stage,G33_CONTROL_VERIFIED,__ATOMIC_RELEASE);
 for(unsigned i=0;i<G33_LPAC_CONTROL;i++)
  if(!g33_lpac_exact(r,io,(enum g33_register)i,empty[i]))return 0;
 if(!g33_lpac_exact(r,io,G33_LPAC_CONTROL,0x300))return 0;
 if(!g33_lpac_issue(r,io,G33_LPAC_MATCH,0x84000001,G33_MATCH_ISSUED))return 0;
 for(unsigned pass=0;pass<2;pass++)for(unsigned i=0;i<G33_REGISTERS;i++)
  if(!g33_lpac_exact(r,io,(enum g33_register)i,live[i]))return 0;
 if(!g33_lpac_guard(r,io))return 0;
 __atomic_store_n(&r->stage,G33_COMPLETE,__ATOMIC_RELEASE);return 1;
}
