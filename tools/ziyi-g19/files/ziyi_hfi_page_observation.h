/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <stdint.h>
/* SM7450 classic PAR format / qualified 36-bit PA window only. Identity is
 * not allocation ownership or Normal-NC qualification. ATTR stays raw. */
int gh_par_identity(uint64_t par,uint64_t va,uint64_t expected_pa);
struct gh_cpu_page_observation {uint64_t va,expected_pa,par,el,sctlr;};
/* Future native read-only observer. No allocator, pmap mutation, software-walk
 * fallback or HFI publication. Clobbers PAR as does the XNU mmu_kvtop routine. */
int gh_cpu_observe(uint64_t va,uint64_t expected_pa,struct gh_cpu_page_observation*);
