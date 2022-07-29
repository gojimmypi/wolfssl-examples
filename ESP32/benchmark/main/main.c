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

#include <wolfcrypt/test/test.h>
#include <wolfcrypt/benchmark/benchmark.h>

#include "sdkconfig.h"
#include "esp_log.h"

#include "time_helper.h"

// #include "unity.h"



#define WOLFSSL_BENCH_ARGV                 CONFIG_BENCH_ARGV
static TickType_t DelayTicks = (10000 / portTICK_PERIOD_MS);

static const char* const TAG = "wolfbenchmark";
char* __argv[22];

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

int construct_argv()
{
    int cnt = 0;
    int i = 0;
    int len = 0;
    char *_argv; /* buffer for copying the string    */
    char *ch; /* char pointer to trace the string */
    char buff[16] = { 0 }; /* buffer for a argument copy       */

    // printf("arg:%s\n", CONFIG_BENCH_ARGV);
    //len = strlen(CONFIG_BENCH_ARGV);
    _argv = (char*)malloc(len + 1);
    if (!_argv) {
        return -1;
    }
    memset(_argv, 0, len + 1);
    //memcpy(_argv, CONFIG_BENCH_ARGV, len);
    _argv[len] = '\0';
    ch = _argv;

    __argv[cnt] = malloc(10);
    sprintf(__argv[cnt], "benchmark");
    __argv[cnt][9] = '\0';
    cnt = 1;

    while (*ch != '\0') {
        /* skip white-space */
        while (*ch == ' ') { ++ch; }

        memset(buff, 0, sizeof(buff));
        /* copy each args into buffer */
        i = 0;
        while ((*ch != ' ') && (*ch != '\0') && (i < 16)) {
            buff[i] = *ch;
            ++i;
            ++ch;
        }
        /* copy the string into argv */
        __argv[cnt] = (char*)malloc(i + 1);
        memset(__argv[cnt], 0, i + 1);
        memcpy(__argv[cnt], buff, i + 1);
        /* next args */
        ++cnt;
    }

    free(_argv);

    return (cnt);
}

#ifndef NO_CRYPT_BENCHMARK
typedef struct func_args {
    int    argc;
    char** argv;
    int    return_code;
} func_args;

static func_args args = { 0 };
#endif

#define WOLFSSL_ESP_IDF_MINIMUM_STACK_SIZE (50 * 1024)
int wolfssl_check()
{
    int ret = 0;
    /* 50KB is a known good stack size with default values of wolfSSL. */
#ifdef CONFIG_ESP_MAIN_TASK_STACK_SIZEs
    ESP_LOGI(TAG, "Configured ESP main task stack size: %d", CONFIG_ESP_MAIN_TASK_STACK_SIZE);
    if (CONFIG_ESP_MAIN_TASK_STACK_SIZE < WOLFSSL_ESP_IDF_MINIMUM_STACK_SIZE) {
        ESP_LOGI(TAG, "Warning: Stack size lower than known good value.");
    }
#else
    ESP_LOGE(TAG, "ERROR: CONFIG_ESP_MAIN_TASK_STACK_SIZE not defined.");
#endif /* CONFIG_ESP_MAIN_TASK_STACK_SIZE */

#ifdef CONFIG_MAIN_TASK_STACK_SIZE
    ESP_LOGI(TAG, "Configured main task stack size: %d", CONFIG_MAIN_TASK_STACK_SIZE);
    if (CONFIG_MAIN_TASK_STACK_SIZE < WOLFSSL_ESP_IDF_MINIMUM_STACK_SIZE) {
        ESP_LOGI(TAG, "Warning: Stack size lower than known good value.");
    }
#else
    ESP_LOGE(TAG, "ERROR: CONFIG_MAIN_TASK_STACK_SIZE not defined.");
#endif /* CONFIG_MAIN_TASK_STACK_SIZE */

#ifdef CONFIG_ESP_TASK_WDT_TIMEOUT_S
    ESP_LOGI(TAG, "Configured CONFIG_ESP_TASK_WDT_TIMEOUT_S: %d", CONFIG_ESP_TASK_WDT_TIMEOUT_S);
#else
    ESP_LOGE(TAG, "ERROR: CONFIG_ESP_TASK_WDT_TIMEOUT_S not defined.");
#endif /* CONFIG_ESP_TASK_WDT_TIMEOUT_S */

#ifdef CONFIG_ESP_INT_WDT_TIMEOUT_MS
    ESP_LOGI(TAG, "Configured CONFIG_ESP_INT_WDT_TIMEOUT_MS: %d", CONFIG_ESP_INT_WDT_TIMEOUT_MS);
#else
    ESP_LOGE(TAG, "ERROR: CONFIG_ESP_INT_WDT_TIMEOUT_MS not defined.");
#endif /* CONFIG_ESP_INT_WDT_TIMEOUT_MS */


#if defined(NO_CRYPT_TEST) && defined(NO_CRYPT_BENCHMARK)
    ret = NOT_COMPILED_IN;
    ESP_LOGI(TAG,
      "Skipped wolfcrypt_test; NO_CRYPT_TEST and NO_CRYPT_BENCHMARK defined.");

    return ret;
#endif // nothing to do

    ESP_LOGI(TAG, "\nCrypt Test\n");
    if ((ret = wolfCrypt_Init()) != 0) {
        ESP_LOGI(TAG, "wolfCrypt_Init failed %d\n", ret);
        return ret;
    }

#ifndef NO_CRYPT_TEST
    #ifdef HAVE_STACK_SIZE
        StackSizeCheck(&args, wolfcrypt_test);
    #else
        wolfcrypt_test(&args);
    #endif

    ret = args.return_code;
    ESP_LOGI(TAG, "Crypt Test: Return code %d\n", ret);

#else
    ret = NOT_COMPILED_IN;
    ESP_LOGI(TAG, "Skipped wolfcrypt_test; NO_CRYPT_TEST defined.")
#endif

#ifndef NO_CRYPT_BENCHMARK

    /* when using atecc608a on esp32-wroom-32se */
    #if defined(WOLFSSL_ESPWROOM32SE) && defined(HAVE_PK_CALLBACKS) \
                                          && defined(WOLFSSL_ATECC508A)
        #if defined(CUSTOM_SLOT_ALLOCATION)
                my_atmel_slotInit();
            /* to register the callback, it needs to be initialized. */
            if ((wolfCrypt_Init()) != 0) {
                ESP_LOGE(TAG, "wolfCrypt_Init failed");
                return;
            }
            atmel_set_slot_allocator(my_atmel_alloc, my_atmel_free);
        #endif
    #endif

    #ifndef NO_CRYPT_BENCHMARK
        printf("\nBenchmark Test\n");

        benchmark_test(&args);
    #endif /* NO_CRYPT_BENCHMARK */

#else /* NO_CRYPT_BENCHMARK is defined, so no benchmark */
    ret = NOT_COMPILED_IN;
#endif /* NO_CRYPT_BENCHMARK */

    wolfCrypt_Cleanup();

    return ret;
}

/* entry point */
void app_main(void)
{
    (void) TAG;
    int ret;

    /* some tests will need a valid time value */
    set_time();

    ret = wolfssl_check();
    ESP_LOGI(TAG, "wolfssl_check: %d", ret);

    for (;;) {
        /* we're not actually doing anything here, other than a heartbeat message */
        ESP_LOGI(TAG, "wolfSSL ESP32 Benchmark: Server main loop heartbeat!");

        taskYIELD();
        vTaskDelay(DelayTicks ? DelayTicks : 1); /* Minimum delay = 1 tick */
    }

}
