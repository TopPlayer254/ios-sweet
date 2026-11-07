/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_hfi_version.h"
int gh_version_command(uint32_t sequence,uint32_t out[2]){
 if(!out)return GN_ARG;uint32_t header;int e=gn_hfi_header(GH_VERSION_COMMAND,2,sequence,&header);if(e)return e;out[0]=header;out[1]=GH_A662_SUPPORTED_VERSION;return GN_OK;
}
int gh_version_reply(const uint32_t ring[GN_HFI_WORDS],const struct gn_hfi_queue*q,uint32_t sent,struct gh_version_reply*out){
 if(!out||(sent&255)!=GH_VERSION_COMMAND||((sent>>8)&255)!=2)return GN_ARG;
 uint32_t status,next;int e=gn_hfi_ack(ring,q,sent,&status,&next);if(e)return e;
 /* Vendor results[2] is status; results[3] is version only on success.
  * A three-word error ACK is meaningful, but a successful one is truncated. */
 if(status){*out=(struct gh_version_reply){status,0,next};return GH_REMOTE_ERROR;}
 unsigned words=(ring[q->read]>>8)&255;if(words<4)return GN_PACKET;
 uint32_t version=ring[(q->read+3)%GN_HFI_WORDS];
 if((version>>28)!=2)return GH_VERSION_MISMATCH;
 *out=(struct gh_version_reply){0,version,next};return GN_OK;
}
