/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * READ-only batching uses the existing owned 32KiB DMA arena's upper 16KiB.
 * No new opcode or DMA span; media writes retain U7's independent 4KiB gate.
 */
#ifndef ZIYI_U8_READ_POLICY_H
#define ZIYI_U8_READ_POLICY_H
#include "u6_transport_policy.h"
#define U8_READ_MAX (U1_DMA_BYTES-U1_DATA_OFF)
static inline int u8_command(struct u1_cmd *out,unsigned op,unsigned lun,uint64_t lba,
                            uint32_t count,uint32_t bs,uint64_t blocks) {
    if(op!=0x28)return u6_command(out,op,lun,lba,count,bs,blocks);
    if(lun>7||(bs!=512&&bs!=4096)||!count||count>U8_READ_MAX/bs||!blocks||
       lba>=blocks||count>blocks-lba||lba>UINT32_MAX||count-1>UINT32_MAX-lba)return 0;
    struct u1_cmd c={{0},(uint8_t)lun,count*bs};c.cdb[0]=0x28;
    u1_putbe32(c.cdb+2,(uint32_t)lba);c.cdb[7]=(uint8_t)(count>>8);c.cdb[8]=(uint8_t)count;
    *out=c;return 1;
}
#endif
