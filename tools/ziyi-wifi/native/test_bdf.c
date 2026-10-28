/* SPDX-License-Identifier: GPL-2.0-only */
#include "bdf.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
 char name[32];
 assert(zwn_bdf_elf(0xff,1,1,name,sizeof(name))&&!strcmp(name,"qca6750/bd_l9sgl.elf"));
 assert(zwn_bdf_elf(0xff,1,0,name,sizeof(name))&&!strcmp(name,"qca6750/bd_l9s.elf"));
 assert(zwn_bdf_elf(0xff,0,0,name,sizeof(name))&&!strcmp(name,"qca6750/bdwlan.elf"));
 assert(zwn_bdf_elf(1,1,1,name,sizeof(name))&&!strcmp(name,"qca6750/bdwlan.e01"));
 assert(zwn_bdf_elf(0xfe,1,1,name,sizeof(name))&&!strcmp(name,"qca6750/bdwlan.efe"));
 assert(zwn_bdf_elf(0x1234,1,1,name,sizeof(name))&&!strcmp(name,"qca6750/bdwlan12.e34"));
 memset(name,0x5a,sizeof(name));
 assert(!zwn_bdf_elf(0xff,1,1,name,31)&&name[0]==0x5a);
 assert(!zwn_bdf_elf(0xff,1,2,name,sizeof(name))&&name[0]==0x5a);
 puts("ZIYI_ICNSS_ELF_BDF_SELECTION_PASS");return 0;
}
