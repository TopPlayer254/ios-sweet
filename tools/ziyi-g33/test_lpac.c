#include "lpac_route.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct model {
 uint32_t value[G33_REGISTERS];
 unsigned operations,fail,guards,guard_fail,writes,orders,ignore,change;
 enum g33_register write_ids[2];
};
static int qualified(void *p,uint64_t epoch,uint64_t table) {
 struct model *m=p;assert(epoch==7&&table==0x900004000ull);
 return ++m->guards!=m->guard_fail;
}
static int read_cb(void *p,enum g33_register id,uint32_t *out) {
 struct model *m=p;assert((unsigned)id<G33_REGISTERS);
 if(++m->operations==m->fail)return 0;
 *out=m->value[id];
 if(m->change&&m->writes&&id==G33_GMU_MATCH)*out^=1;
 return 1;
}
static int write_cb(void *p,enum g33_register id,uint32_t value) {
 struct model *m=p;assert(m->writes<2);
 assert((id==G33_LPAC_CONTROL&&value==0x300&&m->writes==0)||
        (id==G33_LPAC_MATCH&&value==0x84000001&&m->writes==1));
 m->write_ids[m->writes++]=id;
 if(++m->operations==m->fail)return 0;
 if(m->ignore!=m->writes)m->value[id]=value;
 return 1;
}
static int order_cb(void *p) {
 struct model *m=p;m->orders++;return ++m->operations!=m->fail;
}
static struct model fresh(void) {
 struct model m={.value={0x84000005,0x302,0x84000000,0x300,0,0x20000}};return m;
}
static struct g33_io ops(struct model *m) {
 struct g33_io io={m,qualified,read_cb,write_cb,order_cb};return io;
}
int main(void) {
 struct model m=fresh();struct g33_io io=ops(&m);struct g33_record r={0};
 assert(g33_install(&r,&io,7,0x900004000ull));
 assert(r.stage==G33_COMPLETE&&r.error==0&&r.retained&&r.writes==2&&r.before_valid==63&&r.after_valid==63);
 assert(m.writes==2&&m.orders==2&&r.after[G33_LPAC_MATCH]==0x84000001&&r.after[G33_LPAC_CONTROL]==0x300);
 unsigned transactions=m.operations,guards=m.guards,refusals=0;
 assert(!g33_install(&r,&io,7,0x900004000ull)&&m.writes==2);
 for(unsigned at=1;at<=transactions;at++) {
  m=fresh();m.fail=at;io=ops(&m);memset(&r,0,sizeof(r));
  assert(!g33_install(&r,&io,7,0x900004000ull));assert(r.stage==G33_REFUSED&&r.error==G33_IO);
  assert(m.writes<=2&&r.retained==(m.writes!=0));refusals++;
 }
 for(unsigned at=1;at<=guards;at++) {
  m=fresh();m.guard_fail=at;io=ops(&m);memset(&r,0,sizeof(r));
  assert(!g33_install(&r,&io,7,0x900004000ull));assert(r.stage==G33_REFUSED&&r.error==G33_OWNER);
  assert(m.writes<=2&&r.retained==(m.writes!=0));refusals++;
 }
 for(unsigned id=0;id<G33_REGISTERS;id++)for(unsigned bit=0;bit<32;bit++) {
  m=fresh();m.value[id]^=1u<<bit;io=ops(&m);memset(&r,0,sizeof(r));
  assert(!g33_install(&r,&io,7,0x900004000ull));assert(r.error==G33_OCCUPIED&&!m.writes&&!r.retained);refusals++;
 }
 for(unsigned ignore=1;ignore<=2;ignore++) {
  m=fresh();m.ignore=ignore;io=ops(&m);memset(&r,0,sizeof(r));
  assert(!g33_install(&r,&io,7,0x900004000ull));assert(r.error==G33_READBACK&&r.retained);refusals++;
 }
 m=fresh();m.change=1;io=ops(&m);memset(&r,0,sizeof(r));
 assert(!g33_install(&r,&io,7,0x900004000ull));assert(r.error==G33_READBACK&&m.writes==1&&r.retained);refusals++;
 /* SID mask matches precisely 1 and 0x401, including neither secure nor GMU. */
 unsigned matches=0;
 for(unsigned sid=0;sid<2048;sid++)if((sid&~0x400u)==1){assert(sid==1||sid==0x401);matches++;}
 assert(matches==2&&g33_address(G33_LPAC_MATCH)==0x03da0808&&g33_address(G33_LPAC_CONTROL)==0x03da0c08);
 assert(!g33_address((enum g33_register)99));
 memset(&r,0,sizeof(r));assert(!g33_install(&r,&io,0,0x900004000ull));
 assert(!g33_install(&r,&io,7,0x900004001ull));assert(!g33_install(&r,&io,7,1ull<<36));
 printf("G33_LPAC_HOST_PASS refusals=%u transactions=%u guards=%u writes=2 native=0\n",refusals,transactions,guards);
 return 0;
}
