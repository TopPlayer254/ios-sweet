#pragma once
#include "ziyi_wifi_contract.h"
#define ZW_SMEM_BASE 0x80900000ULL
#define ZW_SMEM_BYTES 0x200000ULL
enum { ZW_OK=0,ZW_ARGUMENT,ZW_FORMAT,ZW_BOUNDS,ZW_OVERLAP,ZW_IO,ZW_CHANGED };
typedef int (*zw_read)(void *,uint32_t,void *,uint32_t);
struct zw_smem_record {
 uint32_t error,reads,count,wpss_entry,global_entry;
 uint8_t toc[2][32],entry[2][32][48],partition[2][32][32];
};
/* Callback addresses are offsets within the independently qualified SMEM region.
 * It must not map/guess any peripheral address. No SMP2P allocation or ACK here.
 */

#define ZWIFI_MAX_DESCRIPTORS 2048u
#define ZWIFI_QMP 0x0c300000ULL
#define ZWIFI_IPCC_PENDING 0x0ed18010ULL
#define ZWIFI_CMDDB 0x80860000ULL
#define ZWIFI_CMDDB_BYTES 0x20000u
#define ZWIFI_ITEMS 3u
struct zwifi_locator {uint32_t found,descriptor,data,allocated,logical;uint8_t raw[16];};
struct zwifi_walk {uint32_t error,count,uncached_count,cached_count,end_lane,end_at;uint8_t before[32],after[32],sha256[32];struct zwifi_locator target[3];};
struct zwifi_item {uint32_t found,part,offset,logical,captured,physical_bytes;uint8_t data[340];};
struct zwifi_qmp {uint32_t error,descriptor[15],length,payload[16];};
struct zwifi_db {uint32_t error,reads,scanned,matches,header[36],entry[2][6],aux[2][16];};
struct zwifi_step {uint32_t offset;uint8_t raw[16];};
enum {ZWIFI_NONE=0,ZWIFI_PARTITION_HEADER,ZWIFI_ALLOC_DESCRIPTOR,ZWIFI_TARGET_DESCRIPTOR,ZWIFI_TARGET_PAYLOAD,ZWIFI_QMP_DESCRIPTOR,ZWIFI_QMP_LENGTH,ZWIFI_QMP_PAYLOAD,ZWIFI_IPCC_ID,ZWIFI_DB_HEADER,ZWIFI_DB_ENTRY,ZWIFI_DB_AUX};
struct zwifi_cursor {uint32_t kind,pass,part,count,target,db_slave,db_index,db_entry[6];uint8_t recheck[16];uint8_t scratch_seen[8192];struct zwifi_step history[ZWIFI_MAX_DESCRIPTORS];};
typedef int (*zwifi_reader)(void *,uint64_t,void *,uint32_t);

struct zwifi_record {uint32_t version,bytes,crc,status,stage,denied,adt_facts,survey_complete;uint64_t el,sctlr,frequency,start,end,mpidr_start,mpidr_end;struct zwifi_contract contract;struct zw_smem_record smem;uint32_t extra_reads,survey_error;struct zwifi_walk walk[2][2];struct zwifi_item item[2][3];struct zwifi_qmp qmp[2];struct zwifi_db db[2];uint32_t ipcc_pending[2],smem_version[2];};
struct zwifi_fixed_read {uint64_t physical;uint32_t bytes,reserved;};
static int zwifi_context_valid(const struct zwifi_record*r){return r&&r->adt_facts==ZWIFI_ADT_FACTS&&r->el==4&&(r->sctlr&0x1005)==0x1005&&r->frequency==19200000&&zwifi_contract_valid(&r->contract);}
static int zwifi_entry_kind(const uint8_t*);
static int zwifi_table_validate(const struct zw_smem_record*);
static int zwifi_address_allowed(const struct zwifi_record*,uint64_t,uint32_t);
static int zwifi_survey_allowed(const struct zwifi_record*,struct zwifi_cursor*,uint64_t,uint32_t);
static int zwifi_replay(const struct zwifi_record*,struct zwifi_cursor*,uint32_t*,uint32_t*,struct zwifi_locator[3]);
static int zwifi_survey(struct zwifi_record*,struct zwifi_cursor*,zwifi_reader,void*);
static int zw_smem_observe(zw_read,void*,struct zw_smem_record*);
static void zwifi_sha256(const void*,size_t,uint8_t[32]);
