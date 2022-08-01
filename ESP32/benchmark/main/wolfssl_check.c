#include "wolfssl_check.h"

#include <wolfssl/wolfcrypt/settings.h>
#include <wolfcrypt/test/test.h>
#include <wolfcrypt/benchmark/benchmark.h>

#ifdef WOLFSSL_TRACK_MEMORY
    #include <wolfssl/wolfcrypt/mem_track.h>
#endif

#include "sdkconfig.h"
#include "esp_log.h"

/*
 * Ensure wolfSSL has enough stack space, currently set to a default of 50KB
 *
 * See optimizations to reduce this in other size-constrained environments.
 */
#define WOLFSSL_ESP_IDF_MINIMUM_STACK_SIZE (50 * 1024)

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

/*
 * environment_check() ensures some critical environment settings are defined
 */
int environment_check()
{
    int ret = 0;
    /* 50KB is a known good stack size with default values of wolfSSL. */
#ifdef CONFIG_ESP_MAIN_TASK_STACK_SIZE
    ESP_LOGI(TAG, "Configured ESP main task stack size: %d", CONFIG_ESP_MAIN_TASK_STACK_SIZE);
    if (CONFIG_ESP_MAIN_TASK_STACK_SIZE < WOLFSSL_ESP_IDF_MINIMUM_STACK_SIZE) {
        ESP_LOGI(TAG, "Warning: Stack size lower than known good value.");
    }
#else
    ret = 1;
    ESP_LOGE(TAG, "ERROR: CONFIG_ESP_MAIN_TASK_STACK_SIZE not defined.");
#endif /* CONFIG_ESP_MAIN_TASK_STACK_SIZE */

#ifdef CONFIG_MAIN_TASK_STACK_SIZE
    ESP_LOGI(TAG, "Configured main task stack size: %d", CONFIG_MAIN_TASK_STACK_SIZE);
    if (CONFIG_MAIN_TASK_STACK_SIZE < WOLFSSL_ESP_IDF_MINIMUM_STACK_SIZE) {
        ESP_LOGI(TAG, "Warning: Stack size lower than known good value.");
    }
#else
    ret = 1;
    ESP_LOGE(TAG, "ERROR: CONFIG_MAIN_TASK_STACK_SIZE not defined.");
#endif /* CONFIG_MAIN_TASK_STACK_SIZE */

    /* for WDT watchdog settings, see:
     *
     * https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/system/wdts.html
     */

#ifdef CONFIG_ESP_TASK_WDT_TIMEOUT_S
    ESP_LOGI(TAG, "Configured CONFIG_ESP_TASK_WDT_TIMEOUT_S: %d", CONFIG_ESP_TASK_WDT_TIMEOUT_S);
#else
    ret = 1;
    ESP_LOGE(TAG, "ERROR: CONFIG_ESP_TASK_WDT_TIMEOUT_S not defined.");
#endif /* CONFIG_ESP_TASK_WDT_TIMEOUT_S */

#ifdef CONFIG_ESP_INT_WDT_TIMEOUT_MS
    ESP_LOGI(TAG, "Configured CONFIG_ESP_INT_WDT_TIMEOUT_MS: %d", CONFIG_ESP_INT_WDT_TIMEOUT_MS);
#else
    ret = 1;
    ESP_LOGE(TAG, "ERROR: CONFIG_ESP_INT_WDT_TIMEOUT_MS not defined.");
#endif /* CONFIG_ESP_INT_WDT_TIMEOUT_MS */
    return ret;
}

/*
 * the main wolfSSL Test and Benchmark tests
 */
int wolfssl_check()
{
    int ret = 0;
    environment_check();

    esp_log_level_set("*", ESP_LOG_VERBOSE);
    ESP_LOGI(TAG, "Verbose mode!");

#ifdef WOLFSSL_TRACK_MEMORY
    InitMemoryTracker();
    ShowMemoryTracker();
#endif


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


/*
 * wolfcrypt Tests
 */
#if defined(NO_CRYPT_TEST)
    ret = NOT_COMPILED_IN;
    ESP_LOGI(TAG, "Skipped wolfcrypt_test; NO_CRYPT_TEST defined.")
#else
    #ifdef HAVE_STACK_SIZE
        StackSizeCheck(&args, wolfcrypt_test);
    #else
        ret = wolfcrypt_test(&args);
    #endif

    ret = args.return_code;
    ESP_LOGI(TAG, "Crypt Test: Return code %d\n", ret);

    #ifdef WOLFSSL_TRACK_MEMORY
        ShowMemoryTracker();
    #endif

    return ret;
#endif

/*
 * wolfcrypt Benchmark
 */
#if defined(NO_CRYPT_BENCHMARK)
     /* NO_CRYPT_BENCHMARK is defined, so no benchmark */
    ret = NOT_COMPILED_IN;
    ESP_LOGI(TAG, "Skipped Benchmark: NO_CRYPT_BENCHMARK defined.");
#else
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
    printf("\nBenchmark Test\n");

    if (ret == 0) {
        benchmark_test(&args);
    }
    else {
        ESP_LOGI(TAG, "Skipped Benchmark: tests failed");
    }
#endif /* NO_CRYPT_BENCHMARK */


    wolfCrypt_Cleanup();

    return ret;
}

