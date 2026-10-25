/* U25: fixed SM7450 CPU identity / bounded shared-memory proof policy.
 * Topology comes from the stock ziyi DT; actual secondary MIDRs are checked
 * natively. No DVFS, CPU_OFF, suspend, GPU or storage action here. */
#ifndef ZIYI_U25_SMP_POLICY_H
#define ZIYI_U25_SMP_POLICY_H
#include <stdint.h>
#define Z25_CPUS 8u
#define Z25_WORDS 16u
static inline int z25_identity(unsigned cpu,uint64_t mpidr,uint64_t midr,
 uint64_t frequency,uint64_t el) {
 if(cpu>=Z25_CPUS||el!=1||frequency!=19200000)return 0;
 /* Ignore architectural MT/U/reserved bits; require exact affinities. */
 if((mpidr&0xff00ffffffULL)!=((uint64_t)cpu<<8))return 0;
 if((midr>>24&255)!=0x41||((midr>>16)&15)!=15)return 0;
 return ((midr>>4)&0xfff)==(cpu<4?0xd46u:0xd47u);
}
static inline uint64_t z25_word(unsigned cpu,unsigned word) {
 return 0x5a495949534d5038ULL^((uint64_t)(cpu+1)*0x9e3779b97f4a7c15ULL)^((uint64_t)word*0x100000001b3ULL);
}
static inline uint64_t z25_hash(const uint64_t *words) {
 uint64_t h=0xcbf29ce484222325ULL;
 for(unsigned i=0;i<Z25_WORDS;i++)h=(h^words[i])*0x100000001b3ULL;
 return h;
}
#endif
