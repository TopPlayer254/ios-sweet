/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
/* Called by root-owned BSD hook. Default off; one optional worker thread. */
void ziyi_gpu_probe_start(void);
#ifdef __cplusplus
}
#endif
