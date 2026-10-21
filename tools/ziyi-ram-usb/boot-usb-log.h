/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Bounded readonly /dev/kmsg selection. No logging occurs while reading it.
 */
#ifndef ZIYI_BOOT_USB_LOG_H
#define ZIYI_BOOT_USB_LOG_H
#include <string.h>
#define ZIYI_LOG_SLOTS 20U
#define ZIYI_LOG_WIDTH 200U
struct ziyi_boot_usb_log {
 unsigned int seen,kept,next;
 char line[ZIYI_LOG_SLOTS][ZIYI_LOG_WIDTH];
};
static void ziyi_boot_usb_add(struct ziyi_boot_usb_log *log,const char *record) {
 const char *message=strchr(record,';');if(!message)return;message++;
 /* Exclude our own prior reports and noisy initcall names. */
 if(strstr(message,"ZIYI RAM USB ")==message||strstr(message,"calling ")||strstr(message,"initcall "))return;
 if(!strstr(message,"usb")&&!strstr(message,"USB")&&!strstr(message,"dwc3")&&!strstr(message,"phy"))return;
 size_t n=strlen(message);if(n>=ZIYI_LOG_WIDTH)n=ZIYI_LOG_WIDTH-1;
 memcpy(log->line[log->next],message,n);log->line[log->next][n]=0;
 char *end=strchr(log->line[log->next],'\n');if(end)*end=0;
 log->seen++;if(log->kept<ZIYI_LOG_SLOTS)log->kept++;
 log->next=(log->next+1)%ZIYI_LOG_SLOTS;
}
#endif
