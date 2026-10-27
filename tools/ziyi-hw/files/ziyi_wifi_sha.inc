/** SHA-256, FIPS 180-4 algorithm; no firmware callbacks. BSD-2-Clause-Patent. */
/* Portable adaptation of the previously hardware-tested M2 SHA-256. */
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef uint8_t UINT8;
typedef size_t UINTN;
#define CONST const
#define VOID void

static const UINT32 K[64] = {
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};
static UINT32 R (UINT32 X, unsigned N) { return (X >> N) | (X << (32-N)); }
static VOID Block (UINT32 H[8], CONST UINT8 B[64]) {
  UINT32 W[64], A,C,D,E,F,G,J,L,T1,T2;
  unsigned I;
  for (I=0; I<16; I++) W[I]=((UINT32)B[I*4]<<24)|((UINT32)B[I*4+1]<<16)|((UINT32)B[I*4+2]<<8)|B[I*4+3];
  for (I=16; I<64; I++) W[I]=W[I-16]+(R(W[I-15],7)^R(W[I-15],18)^(W[I-15]>>3))+W[I-7]+(R(W[I-2],17)^R(W[I-2],19)^(W[I-2]>>10));
  A=H[0]; C=H[1]; D=H[2]; E=H[3]; F=H[4]; G=H[5]; J=H[6]; L=H[7];
  for (I=0; I<64; I++) {
    T1=L+(R(F,6)^R(F,11)^R(F,25))+((F&G)^(~F&J))+K[I]+W[I];
    T2=(R(A,2)^R(A,13)^R(A,22))+((A&C)^(A&D)^(C&D));
    L=J; J=G; G=F; F=E+T1; E=D; D=C; C=A; A=T1+T2;
  }
  H[0]+=A; H[1]+=C; H[2]+=D; H[3]+=E; H[4]+=F; H[5]+=G; H[6]+=J; H[7]+=L;
}
static VOID zwifi_sha256 (CONST VOID *Data, UINTN Bytes, UINT8 Digest[32]) {
  UINT32 H[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
  CONST UINT8 *P=Data;
  UINT8 Tail[128];
  UINTN I, Full=Bytes & ~(UINTN)63, Remaining=Bytes-Full, TailBytes=Remaining<56?64:128;
  UINT64 Bits=(UINT64)Bytes*8;
  for (I=0; I<Full; I+=64) Block(H,P+I);
  for (I=0; I<TailBytes; I++) Tail[I]=0;
  for (I=0; I<Remaining; I++) Tail[I]=P[Full+I];
  Tail[Remaining]=0x80;
  for (I=0; I<8; I++) Tail[TailBytes-1-I]=(UINT8)(Bits>>(I*8));
  Block(H,Tail); if (TailBytes==128) Block(H,Tail+64);
  for (I=0; I<32; I++) Digest[I]=(UINT8)(H[I/4]>>(24-8*(I%4)));
}
