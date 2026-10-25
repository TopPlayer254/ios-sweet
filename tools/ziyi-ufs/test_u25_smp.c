#include "u25_smp_policy.h"
#include <assert.h>
#include <stdio.h>
static unsigned checks;
#define CHECK(x) do {assert(x);checks++;} while(0)
int main(void) {
 for(unsigned cpu=0;cpu<8;cpu++) {
  uint64_t midr=cpu<4?0x410fd463:0x411fd470;
  CHECK(z25_identity(cpu,0x81000000ULL+(cpu<<8),midr,19200000,1));
  for(unsigned other=0;other<8;other++)if(cpu!=other)CHECK(!z25_identity(cpu,other<<8,midr,19200000,1));
  CHECK(!z25_identity(cpu,cpu<<8,midr,19200000,2));
  CHECK(!z25_identity(cpu,cpu<<8,midr,62500000,1));
  CHECK(!z25_identity(cpu,(cpu<<8)|1,midr,19200000,1));
  CHECK(!z25_identity(cpu,0x100000000ULL|(cpu<<8),midr,19200000,1));
  CHECK(!z25_identity(cpu,cpu<<8,midr^0x01000000,19200000,1));
  CHECK(!z25_identity(cpu,cpu<<8,cpu<4?0x411fd470:0x410fd463,19200000,1));
  uint64_t words[Z25_WORDS];
  for(unsigned w=0;w<Z25_WORDS;w++)words[w]=z25_word(cpu,w);
  uint64_t h=z25_hash(words);
  for(unsigned w=0;w<Z25_WORDS;w++)for(unsigned bit=0;bit<64;bit++) {
   words[w]^=1ULL<<bit;CHECK(z25_hash(words)!=h);words[w]^=1ULL<<bit;
  }
 }
 CHECK(!z25_identity(8,0x800,0x411fd470,19200000,1));
 CHECK(!z25_identity(~0u,0,0x410fd463,19200000,1));
 printf("U25_SMP_POLICY_PASS checks=%u\n",checks);
 return 0;
}
