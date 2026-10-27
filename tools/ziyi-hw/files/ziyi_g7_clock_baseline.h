#pragma once
#include <stdint.h>
#include <stddef.h>
#define G1_RUN "ZIYI-20261025-G1"
#define G1_DB 0x80860000ULL
#define G1_GPUCC 0x03d90000ULL
#define G1_GCC 0x00100000ULL
#define G1_QFPROM 0x221c8000ULL
#define G1_REGS 32u
struct g1_reg {uint64_t base;uint32_t offset;const char *name;};
extern const struct g1_reg g1_registers[G1_REGS];
struct g1_record {
 char magic[8];uint32_t version,bytes,crc,status;char run[32];
 uint64_t dtb,dtb_bytes,facts,el,sctlr,midr,mpidr,frequency,start,end;
 uint64_t esr,elr,far,vector,exceptions,preserved,canary,stage;
 uint64_t reads,denied,scanned,matches;
 uint32_t db[2][36],entry[2][4][6],aux[2][4][16],regs[2][G1_REGS];
};
_Static_assert(sizeof(struct g1_record)==1480,"G1 ABI");
typedef uint32_t (*g1_reader)(void *,uint64_t,uint32_t,int *);
int g1_read_allowed(uint64_t,uint32_t);
int g1_collect(struct g1_record *,g1_reader,void *);
uint32_t g1_crc(const void *,size_t);
void g1_packet(const struct g1_record *,uint32_t,uint8_t[128]);
void g1_draw(volatile uint32_t *,const uint8_t[128]);
