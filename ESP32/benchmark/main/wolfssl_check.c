#include "wolfssl_check.h"

#include <wolfssl/wolfcrypt/settings.h>
#include <wolfcrypt/test/test.h>
#include <wolfcrypt/benchmark/benchmark.h>

#ifdef WOLFSSL_TRACK_MEMORY
    #include <wolfssl/wolfcrypt/mem_track.h>
#endif

#include "sdkconfig.h"
#include "esp_log.h"

static const char* const TAG = "wolfssl_check";

char* __argv[22];

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
    esp_log_level_set("*", ESP_LOG_VERBOSE);
    ESP_LOGI(TAG, "Verbose mode!");

#ifdef WOLFSSL_TRACK_MEMORY
    InitMemoryTracker();
    ShowMemoryTracker();
#endif

    /* 50KB is a known good stack size with default values of wolfSSL. */
#ifdef CONFIG_ESP_MAIN_TASK_STACK_SIZE
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

    /* for WDT watchdog settings, see:
     *
     * https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/system/wdts.html
     */

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

#ifdef WOLFSSL_TRACK_MEMORY
    ShowMemoryTracker();
#endif


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

