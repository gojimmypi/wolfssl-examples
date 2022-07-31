/*
 * Copyright (C) 2006-2022 wolfSSL Inc.
 *
 * This file is part of wolfSSL.
 *
 * wolfSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * wolfSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <wolfssl/wolfcrypt/settings.h>
#include <wolfssl/wolfcrypt/logging.h>
#include <wolfssl/wolfcrypt/error-crypt.h>
#include <wolfssl/wolfcrypt/types.h>
#include <wolfssl/wolfcrypt/sha256.h>

/* optional memory tracking */
/* #define WOLFSSL_TRACK_MEMORY */
#define WOLFSSL_TRACK_MEMORY
#ifdef WOLFSSL_TRACK_MEMORY
    #include <wolfssl/wolfcrypt/mem_track.h>
#endif

#include <esp_log.h>
#include "sdkconfig.h"

#include "time_helper.h"
#include "wolfssl_check.h"

// #include "unity.h"



#define WOLFSSL_BENCH_ARGV                 CONFIG_BENCH_ARGV
static TickType_t DelayTicks = (10000 / portTICK_PERIOD_MS);

static const char* const TAG = "wolfbenchmark";

#if defined(WOLFSSL_ESPWROOM32SE) && defined(HAVE_PK_CALLBACKS) \
                                  && defined(WOLFSSL_ATECC508A)

#include "wolfssl/wolfcrypt/port/atmel/atmel.h"

/* when you need to use a custom slot allocation, */
/* enable the definition CUSTOM_SLOT_ALLOCAION.   */
#if defined(CUSTOM_SLOT_ALLOCATION)

static byte mSlotList[ATECC_MAX_SLOT];

/* initialize slot array */
void my_atmel_slotInit()
{
    int i;
    for (i = 0; i < ATECC_MAX_SLOT; i++) {
        mSlotList[i] = ATECC_INVALID_SLOT;
    }
}

/* allocate slot depending on slotType */
int my_atmel_alloc(int slotType)
{
    int i, slot = -1;

    switch (slotType) {
    case ATMEL_SLOT_ENCKEY:
        slot = 4;
        break;
    case ATMEL_SLOT_DEVICE:
        slot = 0;
        break;
    case ATMEL_SLOT_ECDHE:
        slot = 0;
        break;
    case ATMEL_SLOT_ECDHE_ENC:
        slot = 4;
        break;
    case ATMEL_SLOT_ANY:
        for (i = 0; i < ATECC_MAX_SLOT; i++) {
            if (mSlotList[i] == ATECC_INVALID_SLOT) {
                slot = i;
                break;
            }
        }
    }

    return slot;
}

/* free slot array       */
void my_atmel_free(int slotId)
{
    if (slotId >= 0 && slotId < ATECC_MAX_SLOT) {
        mSlotList[slotId] = ATECC_INVALID_SLOT;
    }
}

#endif /* CUSTOM_SLOT_ALLOCATION                                       */
#endif /* WOLFSSL_ESPWROOM32SE && HAVE_PK_CALLBACK && WOLFSSL_ATECC508A */

#include "wolfssl/wolfcrypt/port/Espressif/esp32-crypt.h"

void test_sha()
{
    const char* data = "0/0/0/0/0"; // "Hello world" from web : 64:ec:88:ca:00:b2:68:e5:ba:1a:35:67:8a:1b:53:16:d2:12:f4:f3:66:b2:47:72:32:53:4a:8a:ec:a3:7f:3c
    Sha256 sha256[1];  // 3 from web - 4e:07:40:85:62:be:db:8b:60:ce:05:c1:de:cf:e3:ad:16:b7:22:30:96:7d:e0:1f:64:0b:7e:47:29:b4:9f:ce
    byte * hash[100];  // 0 from web - 5f:ec:eb:66:ff:c8:6f:38:d9:52:78:6c:6d:69:6c:79:c2:db:c2:39:dd:4e:91:b4:67:29:d7:3a:27:fb:57:e9

     int ret;
    if ((ret = wc_InitSha256(sha256)) != 0) {
        ESP_LOGE(TAG, "wc_InitSha256 failed");
    }

    wc_Sha256Update(sha256, (byte*)data, sizeof(&data));
    wc_Sha256GetHash(sha256, hash);

    wc_Sha256Update(sha256, (byte*)data, sizeof(&data));
    wc_Sha256GetHash(sha256, hash);
}
/* entry point */
void app_main(void)
{
    (void) TAG;
    int ret;
//    test_sha();

    /* some tests will need a valid time value */
    set_time();
    esp_log_level_set("*", ESP_LOG_VERBOSE);

    ret = wolfssl_check();
    ESP_LOGI(TAG, "wolfssl_check: %d", ret);

    for (;;) {
        /* we're not actually doing anything here, other than a heartbeat message */
        ESP_LOGI(TAG, "wolfSSL ESP32 Benchmark: Server main loop heartbeat!");

        taskYIELD();
        vTaskDelay(DelayTicks ? DelayTicks : 1); /* Minimum delay = 1 tick */
    }

}
