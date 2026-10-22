/* U7 prepared source policy. Requires complete CI/binary/phone audit.
 * Writes require BOTH complete initial root hash and GPT identity proof.
 * Bounds expose only the 3GiB/8GiB image prefix, never all userdata.
 */
#ifndef ZIYI_U7_WRITE_POLICY_H
#define ZIYI_U7_WRITE_POLICY_H
#include "u1_core.h"
struct u7_volume {
    uint64_t first,last,image_bytes;
    uint32_t physical;
    unsigned lun,root_verified,gpt_verified,writable;
};
static inline int u7_volume_valid(const struct u7_volume *v) {
    if(v->lun>7||(v->physical!=512&&v->physical!=4096)||v->first>v->last||v->last==UINT64_MAX||
       (v->image_bytes!=(3ull<<30)&&v->image_bytes!=(8ull<<30)))return 0;
    return v->last-v->first+1>=v->image_bytes/v->physical;
}
static inline int u7_write_command(struct u1_cmd *out,const struct u7_volume *v,uint64_t lba,uint32_t blocks) {
    if(!u7_volume_valid(v)||v->root_verified!=1||v->gpt_verified!=1||v->writable!=1||
       !blocks||blocks>U1_BLOCK_MAX/v->physical||lba<v->first||lba>UINT32_MAX||
       blocks-1>UINT32_MAX-lba)return 0;
    uint64_t offset=lba-v->first,image_blocks=v->image_bytes/v->physical;
    if(offset>=image_blocks||blocks>image_blocks-offset)return 0;
    struct u1_cmd c={{0},(uint8_t)v->lun,blocks*v->physical};
    c.cdb[0]=0x2a;c.cdb[1]=8; /* WRITE(10), Force Unit Access, no other flags. */
    u1_putbe32(c.cdb+2,(uint32_t)lba);c.cdb[7]=(uint8_t)(blocks>>8);c.cdb[8]=(uint8_t)blocks;
    *out=c;return 1;
}
#endif
