#include "ziyi_g7_clock.h"
uint32_t g7_clock_crc(const void *data,size_t bytes){const uint8_t*p=data;uint32_t c=~0u;for(size_t i=0;i<bytes;i++){c^=p[i];for(unsigned k=0;k<8;k++)c=(c>>1)^((0u-(c&1))&0xedb88320u);}return ~c;}
