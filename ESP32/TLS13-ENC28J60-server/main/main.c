/* ENC28J60 Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/*
 *
 *  see https://www.wolfssl.com/docs/quickstart/
 **/

#define WOLFSSL_ESPIDF
#define WOLFSSL_ESPWROOM32
#define WOLFSSL_USER_SETTINGS

#include "sdkconfig.h"

/* wolfSSL */
#include <wolfssl/wolfcrypt/settings.h> /* includes wolfSSL user-settings.h */
#include <wolfssl/wolfcrypt/port/Espressif/esp32-crypt.h>
#ifndef WOLFSSL_ESPIDF
    #warning "Problem with wolfSSL user_settings."
    #warning "Check components/wolfssl/include"
#endif

#include "tls_smp_server_task.h"


/* the usual suspects */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "enc28j60.h"
#include "driver/spi_master.h"

#include "nvs_flash.h"
#if ESP_IDF_VERSION_MAJOR >= 4
// #include "protocol_examples_common.h"
#endif

/* socket includes */
#include "lwip/netdb.h"
#include "lwip/sockets.h"

/* time */
#include  <lwip/apps/sntp.h>

/* wolfSSL */
#include <wolfssl/wolfcrypt/settings.h> // make sure this appears before any other wolfSSL headers
#include <wolfssl/ssl.h>

#ifdef WOLFSSL_TRACK_MEMORY
#include <wolfssl/wolfcrypt/mem_track.h>
#endif

/**
 ******************************************************************************
 ******************************************************************************
 ** USER APPLICATION SETTINGS BEGIN
 ******************************************************************************
 ******************************************************************************
 **/

/* ENC28J60 doesn't burn any factory MAC address, we need to set it manually.
   02:00:00 is a Locally Administered OUI range so should not be used except when testing on a LAN under your control.
*/
uint8_t myMacAddress[] = {
    0x02,
    0x00,
    0x00,
    0x12,
    0x34,
    0x56
};

/* ESP lwip */
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY



/* include certificates. Note that there is an experiation date!
 *
 * See also https://github.com/wolfSSL/wolfssl/blob/master/wolfssl/certs_test.h

   for example:

    #define USE_CERT_BUFFERS_2048
    #include <wolfssl/certs_test.h>
*/

/*
* for reference, embedded client uses:
*
* #include "embedded_CA_FILE.h"
* #include "embedded_CERT_FILE.h"
* #include "embedded_KEY_FILE.h"
*/

/*
server file system versions:
#define CA_FILE   "../../../../certs/client-cert.pem"
#define CERT_FILE "../../../../certs/server-cert.pem"
#define KEY_FILE  "../../../../certs/server-key.pem"
*/


static const char *TAG = "eth_example";

// see https://tf.nist.gov/tf-cgi/servers.cgi
const int NTP_SERVER_COUNT = 3;
const char* ntpServerList[] = {
    "pool.ntp.org",
    "time.nist.gov",
    "utcnist.colorado.edu"
};
const char * TIME_ZONE = "PST-8";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

TickType_t DelayTicks = 5000 / portTICK_PERIOD_MS;
/**
 ******************************************************************************
 ******************************************************************************
 ** USER SETTINGS END
 ******************************************************************************
 ******************************************************************************
 **/





/** Event handler for Ethernet events */
static void eth_event_handler(void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data) {
    uint8_t mac_addr[6] = { 0 };
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        /*
         * see ESP-IDF 5.0 note at
         * https://docs.espressif.com/projects/esp-idf/en/latest/esp32/migration-guides/ethernet.html
         *
         * esp_eth_ioctl() third argument could take int (bool) number as an input in some cases.
         * However, it was not properly documented and, in addition, the number had to be “unnaturally”
         * type casted to void * datatype to prevent compiler warnings as shown in below example:
         *
         * esp_eth_ioctl(eth_handle, ETH_CMD_S_FLOW_CTRL, (void *)true);
         *
         * This could lead to misuse of the esp_eth_ioctl(). Therefore, ESP-IDF 5.0 unified usage of
         * esp_eth_ioctl(). Its third argument now always acts as pointer to a memory location of specific
         * type from/to where the configuration option is read/stored.
         *
         * TODO Migrate Ethernet Drivers to ESP-IDF 5.0

        eth_duplex_t new_duplex_mode = ETH_DUPLEX_HALF;
        esp_eth_ioctl(eth_handle, ETH_CMD_S_DUPLEX_MODE, &new_duplex_mode);

         */
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);

        ESP_LOGI(TAG, "Ethernet Link Up");
        ESP_LOGI(TAG,
            "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
            mac_addr[0],
            mac_addr[1],
            mac_addr[2],
            mac_addr[3],
            mac_addr[4],
            mac_addr[5]);
        break;

    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        break;

    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;

    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;

    default:
        ESP_LOGI(TAG, "Other");
        break;
    }
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");
}

int set_time() {
    /* we'll also return a result code of zero */
    int res = 0;

    //*ideally, we'd like to set time from network, but let's set a default time, just in case */
    struct tm timeinfo;
    timeinfo.tm_year = 2022 - 1900;
    timeinfo.tm_mon = 3;
    timeinfo.tm_mday = 15;
    timeinfo.tm_hour = 8;
    timeinfo.tm_min = 03;
    timeinfo.tm_sec = 10;
    time_t t;
    t = mktime(&timeinfo);

    struct timeval now = { .tv_sec = t };
    settimeofday(&now, NULL);

    /* set timezone */
    setenv("TZ", TIME_ZONE, 1);
    tzset();

    /* next, let's setup NTP time servers
     *
     * see https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/system_time.html#sntp-time-synchronization
    */
    sntp_setoperatingmode(SNTP_OPMODE_POLL);

    int i = 0;
    for (i = 0; i < NTP_SERVER_COUNT; i++) {
        const char* thisServer = ntpServerList[i];
        if (strncmp(thisServer, "\x00", 1)) {
            /* just in case we run out of NTP servers */
            break;
        }
        sntp_setservername(i, thisServer);
    }
    sntp_init();
    return res;
}



int init_ENC28J60() {
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    // Initialize TCP/IP network interface (should be called only once in application)
    ESP_ERROR_CHECK(esp_netif_init());
    // Create default event loop that running in background
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);


    spi_bus_config_t buscfg = {
        .miso_io_num = CONFIG_EXAMPLE_ENC28J60_MISO_GPIO,
        .mosi_io_num = CONFIG_EXAMPLE_ENC28J60_MOSI_GPIO,
        .sclk_io_num = CONFIG_EXAMPLE_ENC28J60_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(CONFIG_EXAMPLE_ENC28J60_SPI_HOST, &buscfg, 1));
    /* ENC28J60 ethernet driver is based on spi driver */
    spi_device_interface_config_t devcfg = {
        .command_bits = 3,
        .address_bits = 5,
        .mode = 0,
        .clock_speed_hz = CONFIG_EXAMPLE_ENC28J60_SPI_CLOCK_MHZ * 1000 * 1000,
        .spics_io_num = CONFIG_EXAMPLE_ENC28J60_CS_GPIO,
        .queue_size = 20
    };
    spi_device_handle_t spi_handle = NULL;
    ESP_ERROR_CHECK(spi_bus_add_device(CONFIG_EXAMPLE_ENC28J60_SPI_HOST, &devcfg, &spi_handle));

    eth_enc28j60_config_t enc28j60_config = ETH_ENC28J60_DEFAULT_CONFIG(spi_handle);
    enc28j60_config.int_gpio_num = CONFIG_EXAMPLE_ENC28J60_INT_GPIO;

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
#if ESP_IDF_VERSION_MAJOR < 5
    mac_config.smi_mdc_gpio_num = -1; // ENC28J60 doesn't have SMI interface
    mac_config.smi_mdio_gpio_num = -1;
#endif
    esp_eth_mac_t *mac = esp_eth_mac_new_enc28j60(&enc28j60_config, &mac_config);

    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.autonego_timeout_ms = 0; // ENC28J60 doesn't support auto-negotiation
    phy_config.reset_gpio_num = -1; // ENC28J60 doesn't have a pin to reset internal PHY
    esp_eth_phy_t *phy = esp_eth_phy_new_enc28j60(&phy_config);

    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&eth_config, &eth_handle));


    mac->set_addr(mac, myMacAddress);


    /* attach Ethernet driver to TCP/IP stack */
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));

    /* Register user defined event handers
     * "ensure that they register the user event handlers as the last thing prior to starting the Ethernet driver."
    */
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    /* start Ethernet driver state machine */
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));

    return 0;
}


void app_main(void) {
    init_ENC28J60();

    // one of the most important aspects of security is the time and date values
    set_time();

    for (;;) {
        ESP_LOGI(TAG, "main loop");
        vTaskDelay(DelayTicks ? DelayTicks : 1); /* Minimum delay = 1 tick */
        tls_smp_server_task();
    }
}
