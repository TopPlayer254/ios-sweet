/* Open infrastructure BSS policy; no fallback from requested WPA2 to open. */
#ifndef ZIYI_OPEN_JOIN_H
#define ZIYI_OPEN_JOIN_H
static inline int
zj_open_bss(unsigned capability, const unsigned char *ies, unsigned length)
{
	unsigned at = 0;
	if (!(capability & 1) || (capability & 0x10) || (length && ies == 0))
		return 0;
	while (at < length) {
		unsigned id, size;
		if (length - at < 2)
			return 0;
		id = ies[at]; size = ies[at + 1]; at += 2;
		if (size > length - at || id == 48)
			return 0;
		if (id == 221 && size >= 4 && ies[at] == 0 && ies[at + 1] == 0x50 &&
		    ies[at + 2] == 0xf2 && ies[at + 3] == 1)
			return 0;
		at += size;
	}
	return 1;
}
static inline unsigned
zj_assoc_capability(unsigned advertised, int secured)
{
	return 1u | (secured ? 0x10u : 0u) | (advertised & 0x0520u);
}
/* Same 22 WPA2 bytes as the pinned station; open has no RSN element. */
static inline unsigned
zj_write_rsn(unsigned char *rsn, unsigned char *out, unsigned group_cipher)
{
	static const unsigned char base[22] = {48,20,1,0,0,15,172,4,1,0,0,15,172,4,1,0,0,15,172,2,0,0};
	unsigned i;
	if (group_cipher == 0)
		return 0;
	for (i = 0; i < 22; i++) rsn[i] = base[i];
	rsn[7] = (unsigned char)group_cipher;
	for (i = 0; i < 22; i++) out[i] = rsn[i];
	return 22;
}
#endif
