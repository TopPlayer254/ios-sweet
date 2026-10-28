/* SPDX-License-Identifier: GPL-2.0-only */
#include "smmu.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct zws_snapshot a,b;
static struct zws_choice c;
static unsigned checks;
#define CHECK(x) do{assert(x);checks++;}while(0)
static void reset(void){memset(&a,0,sizeof(a));a.scr0=1u<<10;a.idr0=128;a.idr1=(6u<<28)|128u;b=a;}
int main(void)
{
 reset();CHECK(zws_select(&a,&b,&c)&&c.match==0&&c.context==0&&c.page==4096&&c.pages==128);
 for(unsigned n=0;n<ZWS_SMRS;n++){reset();b.smr[n]=1;CHECK(!zws_select(&a,&b,&c));reset();b.s2cr[n]=1;CHECK(!zws_select(&a,&b,&c));}
 for(unsigned n=0;n<ZWS_CBS;n++){reset();b.sctlr[n]=1;CHECK(!zws_select(&a,&b,&c));}
 reset();a.sctlr[0]=b.sctlr[0]=1;CHECK(zws_select(&a,&b,&c)&&c.context==1);
 reset();for(unsigned n=0;n<ZWS_CBS;n++)a.sctlr[n]=b.sctlr[n]=1;CHECK(!zws_select(&a,&b,&c));
 reset();for(unsigned n=0;n<ZWS_SMRS;n++)a.smr[n]=b.smr[n]=0x80000001;CHECK(!zws_select(&a,&b,&c));
 reset();a.smr[0]=b.smr[0]=0x80000001;a.s2cr[0]=b.s2cr[0]=0;CHECK(zws_select(&a,&b,&c)&&c.match==1&&c.context==1);
 reset();a.smr[0]=b.smr[0]=0x80000001;a.s2cr[0]=b.s2cr[0]=1u<<16;CHECK(zws_select(&a,&b,&c)&&c.context==0);
 reset();a.smr[0]=b.smr[0]=0x80011c00;CHECK(!zws_select(&a,&b,&c));
 reset();a.smr[0]=b.smr[0]=0xfff80000;CHECK(!zws_select(&a,&b,&c));
 reset();a.smr[0]=b.smr[0]=0x80002800;a.s2cr[0]=b.s2cr[0]=3u<<16;CHECK(!zws_select(&a,&b,&c));
 reset();a.scr0=b.scr0=9;CHECK(!zws_select(&a,&b,&c));
 reset();a.idr0=b.idr0=119;CHECK(!zws_select(&a,&b,&c));
 reset();a.idr1=b.idr1=(6u<<28)|77u;CHECK(!zws_select(&a,&b,&c));
 reset();a.idr1=b.idr1=(5u<<28)|128u;CHECK(!zws_select(&a,&b,&c));
 reset();a.smr[0]=b.smr[0]=0x84022800;a.s2cr[0]=b.s2cr[0]=77;CHECK(zws_select(&a,&b,&c)&&c.match==1&&c.context==0);
 CHECK(!zws_select(NULL,&b,&c));CHECK(!zws_select(&a,NULL,&c));CHECK(!zws_select(&a,&b,NULL));
 printf("Wi-Fi HLOS SMMU selection PASS %u checks; model only, no native DMA\n",checks);return 0;
}
