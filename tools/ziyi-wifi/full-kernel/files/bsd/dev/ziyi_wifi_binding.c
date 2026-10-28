/* SPDX-License-Identifier: MIT
 * Native SWEET source contract binding. A coldboot bootarg is an explicit
 * operator attestation, not a hardware OFF detector. Retained DSP state is
 * independently rejected by the transport before any PAS INIT operation.
 */
#include <pexpert/arm64/board_config.h>
#include <pexpert/pexpert.h>
#include <sys/systm.h>
#include <kern/clock.h>
#include "ziyi_wifi_native_profile.h"
static struct zadt_binding wifi_dt;
static unsigned wifi_bound; /* 0 unused, 1 opening, 2 bound, 3 permanently failed */
static uint64_t wifi_epoch;
int ziyi_wifi_native_binding(int initial);
int ziyi_wifi_native_boot_authorized(void);
uint64_t ziyi_wifi_native_epoch(void);
extern int ziyi_wifi_bdf_begin(uint64_t,int);
static int cpu_context(void)
{
 uint64_t el,sctlr,frequency;
 __asm__ volatile("mrs %0,CurrentEL":"=r"(el));
 __asm__ volatile("mrs %0,sctlr_el1":"=r"(sctlr));
 __asm__ volatile("mrs %0,cntfrq_el0":"=r"(frequency));
 return el==4&&(sctlr&0x1005)==0x1005&&frequency==19200000;
}
int ziyi_wifi_native_binding(int initial)
{
 unsigned state=__atomic_load_n(&wifi_bound,__ATOMIC_ACQUIRE);
 if(initial&&state==0){
  unsigned zero=0;if(!__atomic_compare_exchange_n(&wifi_bound,&zero,1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 0;
  int ok=cpu_context()&&zadt_open(&wifi_dt,"/arm-io/ziyi-wifi-native",zwn_adt_properties,ZWN_ADT_COUNT);
  wifi_epoch=mach_absolute_time();if(!wifi_epoch)ok=0;
  state=ok?2u:3u;__atomic_store_n(&wifi_bound,state,__ATOMIC_RELEASE);
 }
 if(state!=2||!cpu_context())return 0;
 /* Read-only shared snapshot; concurrent readers use a local scratch copy. */
 struct zadt_binding check=wifi_dt;
 if(!zadt_recheck(&check,"/arm-io/ziyi-wifi-native",zwn_adt_properties,ZWN_ADT_COUNT)){
  __atomic_store_n(&wifi_bound,3u,__ATOMIC_RELEASE);return 0;
 }
 return __atomic_load_n(&wifi_bound,__ATOMIC_ACQUIRE)==2;
}
uint64_t ziyi_wifi_native_epoch(void){return ziyi_wifi_native_binding(0)?wifi_epoch:0;}
int ziyi_wifi_native_boot_authorized(void)
{
 unsigned enabled=0,cold=0;
 if(!PE_parse_boot_argn("wifi",&enabled,sizeof(enabled))||enabled!=1||
    !PE_parse_boot_argn("ziyi_wifi_coldboot",&cold,sizeof(cold))||cold!=1)return 0;
 if(!ziyi_wifi_native_binding(1)||!ziyi_wifi_bdf_begin(wifi_epoch,ZWN_NATIVE_SKU_GLOBAL))return 0;
 printf("ZWIFI native ziyi/GL DT bound; coldboot operator attested, remote state still checked\n");
 return 1;
}
