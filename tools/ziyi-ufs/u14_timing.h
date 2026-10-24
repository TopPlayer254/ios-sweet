/* Absolute vsync phase. Late rendering skips expired timer deadlines, not
 * pending frames. A single latest swap is retained; no catch-up storm. */
#ifndef ZIYI_U14_TIMING_H
#define ZIYI_U14_TIMING_H
#include <stdint.h>
static inline uint64_t zv_next_deadline(uint64_t previous,uint64_t now,uint64_t period) {
 if(!period||previous>UINT64_MAX-period)return 0;
 uint64_t next=previous+period;
 if(next<=now) {
  uint64_t missed=(now-next)/period+1;
  if(missed>(UINT64_MAX-next)/period)return 0;
  next+=missed*period;
 }
 return next;
}
#endif
