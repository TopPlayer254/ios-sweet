/* SPDX-License-Identifier: GPL-2.0-only
 * Bridge the real ath11k capabilities callback to our RAM firmware provider.
 * Native startup must bind the exact ziyi region before starting ath11k.
 */
#include <sys/errno.h>
#include <libkern/libkern.h>
#include <os/atomic_private.h>
#include "ziyi_wifi_bdf.h"
extern int ziyi_wifi_native_binding(int initial);
extern const uint8_t *ziyi_wifi_image_file(const char*,const char*,uint64_t*);
static unsigned phase; /* 0 unused, 1 bound, 2 selecting, 3 selected, 4 failed */
static uint64_t epoch;
static int global_region;
static uint32_t target_board;
static char board_file[32];

/* Region is the actual hardware SKU (0=ziyi, 1=ziyigl). This call belongs to
 * the native DT/boot provenance binding, not an arbitrary userland preference.
 * It must happen once, before driver threads can ask for a file. */
int ziyi_wifi_bdf_begin(uint64_t boot_epoch,int ziyigl);
int lx_os_qmi_target(unsigned int board_id);
const uint8_t *ziyi_wifi_driver_file(const char*,uint64_t*);
int ziyi_wifi_bdf_begin(uint64_t boot_epoch,int ziyigl)
{
 if(!boot_epoch||(ziyigl!=0&&ziyigl!=1)||!ziyi_wifi_native_binding(0)||
    !os_atomic_cmpxchg(&phase,0u,2u,acq_rel))return 0;
 epoch=boot_epoch;global_region=ziyigl;os_atomic_store(&phase,1u,release);return 1;
}
int lx_os_qmi_target(unsigned int board_id)
{
 if(!ziyi_wifi_native_binding(0)||!epoch)return ENXIO;
 unsigned state=os_atomic_load(&phase,acquire);
 if(state==3)return target_board==board_id?0:EINVAL;
 if(!os_atomic_cmpxchg(&phase,1u,2u,acq_rel))return ENXIO;
 uint64_t size=0;
 if(!zwn_bdf_elf(board_id,1,global_region,board_file,sizeof(board_file))||
    !ziyi_wifi_image_file("fw",board_file,&size)||!size){
  os_atomic_store(&phase,4u,release);return ENOENT;
 }
 target_board=board_id;os_atomic_store(&phase,3u,release);return 0;
}
const uint8_t *ziyi_wifi_driver_file(const char*name,uint64_t*size)
{
 if(size)*size=0;
 if(!name||!size||!ziyi_wifi_native_binding(0)||os_atomic_load(&phase,acquire)!=3)return NULL;
 static const char prefix[]="ath11k/WCN6750/hw1.0/";
 const char*base=name;
 if(!strncmp(name,prefix,sizeof(prefix)-1))base+=sizeof(prefix)-1;
 if(!strcmp(base,"board.bin"))return ziyi_wifi_image_file("fw",board_file,size);
 if(!strcmp(base,"regdb.bin"))return ziyi_wifi_image_file("fw","qca6750/regdb_xiaomi.bin",size);
 /* board-2.bin and firmware feature containers are optional; never substitute
  * an unrelated blob for a format or board that the firmware did not request. */
 return NULL;
}
