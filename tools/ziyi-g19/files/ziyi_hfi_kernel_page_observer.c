/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_hfi_page_observation.h"
int gh_cpu_observe(uint64_t va,uint64_t expected,struct gh_cpu_page_observation*out){
 if(!out||!va||!expected||expected>=(1ULL<<36)||(va&4095)!=(expected&4095))return 0;
 uint64_t el,sctlr;__asm__ volatile("mrs %0, CurrentEL":"=r"(el));if(el!=4)return 0;
 __asm__ volatile("mrs %0, sctlr_el1":"=r"(sctlr));if(!(sctlr&1u))return 0;
 uint64_t prior_daif,par;
 /* Match captured XNU machine_routines_asm.s:mmu_kvtop: IRQ+FIQ masked for
  * AT/ISB/PAR, exact prior DAIF restored. No C call can overwrite PAR here. */
 __asm__ volatile("mrs %0, daif\n\tmsr daifset, #3\n\tat s1e1r, %2\n\tisb sy\n\tmrs %1, par_el1\n\tmsr daif, %0":"=&r"(prior_daif),"=&r"(par):"r"(va):"memory");
 *out=(struct gh_cpu_page_observation){va,expected,par,el,sctlr};
 return gh_par_identity(par,va,expected);
}
