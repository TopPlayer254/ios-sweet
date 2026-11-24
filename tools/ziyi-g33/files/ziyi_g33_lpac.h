/* PC-qualified candidate. No native adapter is connected to this module yet. */
#pragma once
#include <stdint.h>

enum g33_register { G33_GMU_MATCH, G33_GMU_CONTROL, G33_CORE_MATCH,
 G33_CORE_CONTROL, G33_LPAC_MATCH, G33_LPAC_CONTROL, G33_REGISTERS };
enum g33_stage { G33_NEW, G33_SNAPSHOT, G33_CONTROL_ISSUED,
 G33_CONTROL_VERIFIED, G33_MATCH_ISSUED, G33_COMPLETE, G33_REFUSED };
enum g33_error { G33_OK, G33_ARGUMENT, G33_OWNER, G33_IO, G33_OCCUPIED, G33_READBACK };
struct g33_record {
 uint64_t epoch, table_pa;
 uint32_t stage, failed_stage, error, retained, reads, writes;
 uint32_t before[G33_REGISTERS], after[G33_REGISTERS];
 uint32_t before_valid, after_valid;
};
struct g33_io {
 void *ctx;
 /* Must freshly qualify the original lease, epoch, live GMU/GX/core tables,
  * NC backing and CP cohort. The two owned LPAC words are checked here. */
 int (*guard)(void *, uint64_t epoch, uint64_t core_table_pa);
 int (*read)(void *, enum g33_register, uint32_t *);
 int (*write)(void *, enum g33_register, uint32_t);
 int (*order)(void *);
};
uint64_t g33_address(enum g33_register);
int g33_install(struct g33_record *, const struct g33_io *, uint64_t epoch,
 uint64_t core_table_pa);
