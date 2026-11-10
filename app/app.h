/* SPDX-License-Identifier: MPL-2.0
 * Platform contract adapted from Charles Vestal's fm1-base template. */
#pragma once
#include <stdint.h>
#include "plat.h"
#define APP_NAME "XENOLITH I"
#define APP_SPLASH_RGB RGB(152,0,0)
#define APP_SPLASH
#define APP_BLOCK 256u
/* Preserve the original object IDs and sectors. Individual performance slots
 * use sixteen A/B pairs in the app-data region, away from loader staging. */
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_BLE,
       OBJ_PATCH0, OBJ_PATCH1, OBJ_PATCH2, OBJ_PATCH3,
       OBJ_PATCH4, OBJ_PATCH5, OBJ_PATCH6, OBJ_PATCH7,
       OBJ_PATCH8, OBJ_PATCH9, OBJ_PATCH10, OBJ_PATCH11,
       OBJ_PATCH12, OBJ_PATCH13, OBJ_PATCH14, OBJ_PATCH15, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u,0xDC000u,0xDE000u, \
    0xB0000u,0xB2000u,0xB4000u,0xB6000u,0xB8000u,0xBA000u,0xBC000u,0xBE000u, \
    0xC0000u,0xC2000u,0xC4000u,0xC6000u,0xC8000u,0xCA000u,0xCC000u,0xCE000u}
/* Raw saved objects only. Calibration and BLE identity belong to this unit,
 * and are deliberately excluded from portable performance backups. */
#define APP_BACKUP_MAGIC UINT32_C(0x4b424e58) /* little-endian "XNBK" */
#define APP_BACKUP_VERSION 1u
#define APP_BACKUP_WHAT "replaces saved slots"
#define APP_BACKUP_OBJS {OBJ_SETTINGS,OBJ_PATCH0,OBJ_PATCH1,OBJ_PATCH2,OBJ_PATCH3, \
    OBJ_PATCH4,OBJ_PATCH5,OBJ_PATCH6,OBJ_PATCH7,OBJ_PATCH8,OBJ_PATCH9,OBJ_PATCH10, \
    OBJ_PATCH11,OBJ_PATCH12,OBJ_PATCH13,OBJ_PATCH14,OBJ_PATCH15}
int app_backup_playing(void);
void app_backup_flush(void);
void app_backup_hold(int on);
void app_backup_restored(void);
void app_init(void);
void app_render(int32_t *lr,uint32_t n,uint32_t master_q12);
void app_panic(void);
void app_ui_init(void);
void app_splash(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *a,const char *b);
