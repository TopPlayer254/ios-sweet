/* Installed-volume identity, not a claim that mutable filesystem data is hashed.
 * An installed profile is issued only from the sealed full native install proof.
 */
#ifndef ZIYI_U9_INSTALL_POLICY_H
#define ZIYI_U9_INSTALL_POLICY_H
#include <stdint.h>
#include <stddef.h>
static inline uint32_t u9_be32(const uint8_t *p) {
    return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];
}
static inline int u9_hfs_identity(const uint8_t *p,size_t n,uint64_t bytes,uint8_t identity[28]) {
    if(!p||!identity||n<512||bytes!=(8ull<<30)||p[0]!='H'||p[1]!='X'||p[2]||p[3]!=5)return 0;
    uint32_t block=u9_be32(p+40),count=u9_be32(p+44),journal=u9_be32(p+12);
    if(block!=4096||(uint64_t)block*count!=bytes||!(u9_be32(p+4)&0x2000)||
       !journal||journal>=count||!u9_be32(p+16)||!(u9_be32(p+104)|u9_be32(p+108)))return 0;
    /* Stable fields only: signature/version, creation date, geometry,
     * Finder volume ID and journal-info-block location. Mutable counts,
     * mount status, modification dates and fork extents are excluded. */
    for(unsigned i=0;i<4;i++){identity[i]=p[i];identity[4+i]=p[16+i];identity[24+i]=p[12+i];}
    for(unsigned i=0;i<8;i++){identity[8+i]=p[40+i];identity[16+i]=p[104+i];}
    return 1;
}
static inline int u9_sync_valid(uint64_t image,uint64_t offset,uint64_t length,
                                uint32_t options,const uint8_t reserved[4]) {
    if(image!=(8ull<<30)&&image!=(3ull<<30))return 0;
    if(offset>image||length>image-offset||(options&~2u))return 0;
    for(unsigned i=0;i<4;i++)if(reserved[i])return 0;
    return 1;
}
#endif
