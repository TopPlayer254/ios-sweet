/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_hfi_page_observation.h"
int gh_par_identity(uint64_t par,uint64_t va,uint64_t expected){
 if(!va||!expected||expected>=(1ULL<<36)||(par&1u)||(par&0x00fffff000000000ULL))return 0;
 return ((par&0x0000fffffffff000ULL)|(va&4095u))==expected;
}
