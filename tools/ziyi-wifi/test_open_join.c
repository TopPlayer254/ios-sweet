#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "open_join.h"
static unsigned checks;
#define CHECK(x) do { assert(x); checks++; } while (0)
int main(void)
{
	unsigned char rsn[22], out[26], ies[260];
	const unsigned char expected[22] = {48,20,1,0,0,15,172,4,1,0,0,15,172,4,1,0,0,15,172,2,0,0};
	unsigned cap, id, len, i;
	for (cap=0; cap<65536; cap++) {
		CHECK(zj_open_bss(cap, 0, 0) == ((cap & 1) != 0 && !(cap & 16)));
		CHECK(zj_assoc_capability(cap, 0) == (1u | (cap & 0x520u)));
		CHECK(zj_assoc_capability(cap, 1) == (1u | 16u | (cap & 0x520u)));
	}
	CHECK(!zj_open_bss(1, 0, 1));
	for (id=0; id<256; id++) for (len=0; len<256; len++) {
		memset(ies, 0xa5, sizeof(ies)); ies[0]=(unsigned char)id; ies[1]=(unsigned char)len;
		CHECK(zj_open_bss(1, ies, len+2) == (id != 48));
		CHECK(!zj_open_bss(1, ies, 1));
		if (len) CHECK(!zj_open_bss(1, ies, len+1));
		CHECK(!zj_open_bss(0x11, ies, len+2));
	}
	/* A valid sequence, then every truncated prefix of its second element. */
	memcpy(ies, "\x00\x02" "AB" "\xdd\x04\x00\x50\xf2\x02", 10);
	CHECK(zj_open_bss(1, ies, 10));
	for (i=5; i<10; i++) CHECK(!zj_open_bss(1, ies, i));
	ies[9]=1; CHECK(!zj_open_bss(1, ies, 10)); /* legacy WPA, even if Privacy was absent */
	ies[4]=48; CHECK(!zj_open_bss(1, ies, 10)); /* RSN, never silently downgrade */
	memset(rsn, 0xa5, sizeof(rsn)); memset(out, 0xa5, sizeof(out));
	CHECK(zj_write_rsn(rsn, out+2, 0)==0);
	for (i=0; i<sizeof(rsn); i++) CHECK(rsn[i]==0xa5);
	for (i=0; i<sizeof(out); i++) CHECK(out[i]==0xa5);
	CHECK(zj_write_rsn(rsn, out+2, 4)==22);
	CHECK(memcmp(rsn, expected, 22)==0); CHECK(memcmp(out+2, expected, 22)==0);
	CHECK(out[0]==0xa5 && out[1]==0xa5 && out[24]==0xa5 && out[25]==0xa5);
	printf("ZIYI_OPEN_JOIN_POLICY_PASS %u checks; no native connection claim\n", checks);
	return 0;
}
