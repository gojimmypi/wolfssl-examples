/* ENC28J60 Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#define WOLFSSL_ESPIDF
#define WOLFSSL_ESPWROOM32
#define WOLFSSL_USER_SETTINGS

/* the usual suspects */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <stdio.h>
#include <string.h>
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

/* ESP specific */
#include "wifi_connect.h"

/* socket includes */
#include "lwip/netdb.h"
#include "lwip/sockets.h"

/* time */
#include  <lwip/apps/sntp.h>

/* wolfSSL */
#include <wolfssl/wolfcrypt/settings.h> // make sure this appears before any other wolfSSL headers
#include <wolfssl/ssl.h>
#define USE_CERT_BUFFERS_2048
#include <wolfssl/certs_test.h>

#ifdef WOLFSSL_TRACK_MEMORY
#include <wolfssl/wolfcrypt/mem_track.h>
#endif

#include "embedded_CERT_FILE.h"
#include "embedded_CA_FILE.h"
#include "embedded_KEY_FILE.h"

static const char *TAG = "eth_example";

// #define TLS_SMP_TARGET_HOST              "192.168.1.1"
// #define DEFAULT_PORT                     11111

// see https://tf.nist.gov/tf-cgi/servers.cgi
const int NTP_SERVER_COUNT = 3;
const char* ntpServer1[] = { "pool.ntp.org",
                             "time.nist.gov",
                             "utcnist.colorado.edu"
                           };
const char * TIME_ZONE = "PST-8";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

TickType_t DelayTicks = 5000 / portTICK_PERIOD_MS;
  
/* sed 's/\(.*\)\r/"\1\\n"/g' client-cert.pem */

    

int tls_smp_client_task()
{
    int ret;
    int sockfd;
    int doPeerCheck;
    int sendGet;
    struct sockaddr_in servAddr;
    char buff[256];
    const char* ch = TLS_SMP_TARGET_HOST;
    size_t len;
    struct hostent *hp;
    struct ip4_addr *ip4_addr;
    const char sndMsg[] = "GET /index.html HTTP/1.0\r\n\r\n";

    struct timeval tv_now;
    
    /* see https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/system_time.html#overview */
    ret = gettimeofday(&tv_now, NULL);
    int64_t time_us = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;
    
    /* declare wolfSSL objects */
    WOLFSSL_CTX *ctx;
    WOLFSSL *ssl;

    WOLFSSL_ENTER("tls_smp_client_task");

    
    doPeerCheck = 0;
    sendGet = 1;

#ifdef DEBUG_WOLFSSL
    WOLFSSL_MSG("Debug ON");
    wolfSSL_Debugging_ON();
    //ShowCiphers();
#endif


    esp_log_level_set("*", ESP_LOG_VERBOSE); 
    
    /* Create a socket that uses an internet IPv4 address,
     * Sets the socket to be stream based (TCP),
     * 0 means choose the default protocol. */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        ESP_LOGE(TAG, "ERROR: failed to create the socket\n");
        for (;;)
        {
            ESP_LOGI(TAG, "socket fail loop");
            vTaskDelay(DelayTicks ? DelayTicks : 1); /* Minimum delay = 1 tick */     
        }
    }
    
    /* Initialize the server address struct with zeros */
    memset(&servAddr, 0, sizeof(servAddr));

    /* Fill in the server address */
    servAddr.sin_family = AF_INET; /* using IPv4      */
    servAddr.sin_port   = htons(DEFAULT_PORT); /* on DEFAULT_PORT */    

    
    
    ESP_LOGI(TAG, "get target IP address");

    hp = gethostbyname(TLS_SMP_TARGET_HOST);
    if (!hp) {
        ESP_LOGE(TAG, "Failed to get host name.");
        ip4_addr = NULL;
    }
    else {

        ip4_addr = (struct ip4_addr *)hp->h_addr;
        ESP_LOGI(TAG, IPSTR, IP2STR(ip4_addr));
    }
    

    if (*ch >= '1' && *ch <= '9') {
        /* Get the server IPv4 address from the command line call */
        WOLFSSL_MSG("inet_pton");
        if ((ret = inet_pton(AF_INET,
            TLS_SMP_TARGET_HOST,
            &servAddr.sin_addr)) != 1) {
            ESP_LOGE(TAG, "ERROR: invalid address ret=%d\n", ret);
            for (;;) {
                ESP_LOGI(TAG, "inet_pton file loop");
                vTaskDelay(DelayTicks ? DelayTicks : 1); /* Minimum delay = 1 tick */     
            }                
        }
    }
    else {
        servAddr.sin_addr.s_addr = ip4_addr->addr;
    }
    
    /* Connect to the server */
    if ((ret = connect(sockfd, (struct sockaddr*) &servAddr, sizeof(servAddr)))
         == -1) {
             for (;;) {
                 ESP_LOGI(TAG, "connect fail loop");
                 vTaskDelay(DelayTicks ? DelayTicks : 1); /* Minimum delay = 1 tick */     
             }
         }
    
    /* Initialize wolfSSL */
    if ((ret = wolfSSL_Init()) != WOLFSSL_SUCCESS) {
        for (;;) {
            ESP_LOGI(TAG, "wolfSSL_CTX_new fail loop");
            vTaskDelay(DelayTicks ? DelayTicks : 1); /* Minimum delay = 1 tick */     
        }
    }
    
    /* Create and initialize WOLFSSL_CTX */
//    if ((ctx = wolfSSL_CTX_new(wolfSSLv23_client_method())) == NULL) {
    if ((ctx = wolfSSL_CTX_new(wolfTLSv1_3_client_method())) == NULL) {
        ESP_LOGE(TAG, "ERROR: failed to create WOLFSSL_CTX\n");
        for (;;) {
            ESP_LOGI(TAG, "wolfSSL_CTX_new fail loop");
            vTaskDelay(DelayTicks ? DelayTicks : 1); /* Minimum delay = 1 tick */     
        }
    }
    
    
    /* Load client certificate into WOLFSSL_CTX */
    if ((ret = wolfSSL_CTX_use_certificate_buffer(ctx, CERT_FILE, sizeof_CERT_FILE(), WOLFSSL_FILETYPE_PEM))
        != WOLFSSL_SUCCESS) {
        for (;;) {
            ESP_LOGI(TAG, "wolfSSL_CTX_load_verify_buffer file loop");
            vTaskDelay(DelayTicks ? DelayTicks : 1); /* Minimum delay = 1 tick */     
        }    
    }    
    
    WOLFSSL_MSG("Loading...cert");
    /* Load client certificates into WOLFSSL_CTX */
    /* Load client key into WOLFSSL_CTX */
    if ((ret = wolfSSL_CTX_use_PrivateKey_buffer(ctx, KEY_FILE, sizeof_KEY_FILE(), WOLFSSL_FILETYPE_PEM))
        != WOLFSSL_SUCCESS) {
            for (;;) {
                ESP_LOGI(TAG, "wolfSSL_CTX_use_PrivateKey_buffer fail loop");
                vTaskDelay(DelayTicks ? DelayTicks : 1); /* Minimum delay = 1 tick */     
            }    
    }

    /* Load CA certificate into WOLFSSL_CTX */
    if ((ret = wolfSSL_CTX_load_verify_buffer(ctx, CA_FILE, sizeof_CA_FILE(), WOLFSSL_FILETYPE_PEM))
         != WOLFSSL_SUCCESS) {
        for (;;) {
            ESP_LOGI(TAG, "wolfSSL_CTX_load_verify_locations fail loop");
            vTaskDelay(DelayTicks ? DelayTicks : 1); /* Minimum delay = 1 tick */     
        }    

    }
    
    /* Create a WOLFSSL object */
    if ((ssl = wolfSSL_new(ctx)) == NULL) {
        for (;;) {
            ESP_LOGI(TAG, "wolfSSL_new fail loop");
            vTaskDelay(DelayTicks ? DelayTicks : 1); /* Minimum delay = 1 tick */     
        }    
    }

    /* Attach wolfSSL to the socket */
    if ((ret = wolfSSL_set_fd(ssl, sockfd)) != WOLFSSL_SUCCESS) {
        for (;;) {
            ESP_LOGI(TAG, "wolfSSL_set_fd fail loop");
            vTaskDelay(DelayTicks ? DelayTicks : 1); /* Minimum delay = 1 tick */     
        }    
    }    

    /* Connect to wolfSSL on the server side */
    if ((ret = wolfSSL_connect(ssl)) != WOLFSSL_SUCCESS) {
        for (;;) {
            ESP_LOGI(TAG, "wolfSSL_connect fail loop");
            vTaskDelay(DelayTicks ? DelayTicks : 1); /* Minimum delay = 1 tick */     
        }    
    }

    /* Get a message for the server from stdin */
    printf("Message for server: ");
    memset(buff, 0, sizeof(buff));
    
    char msg[] = "Hello World"; 
    len = strnlen(msg, sizeof(msg));

    /* Send the message to the server */
    if ((ret = wolfSSL_write(ssl, msg, len)) != len) {
        for (;;) {
            ESP_LOGI(TAG, "wolfSSL_write fail loop");
            vTaskDelay(DelayTicks ? DelayTicks : 1); /* Minimum delay = 1 tick */     
        }    
    }

    /* Read the server data into our buff array */
    memset(buff, 0, sizeof(buff));
    if ((ret = wolfSSL_read(ssl, buff, sizeof(buff) - 1)) < 0) {
        for (;;) {
            ESP_LOGI(TAG, "wolfSSL_read fail loop");
            vTaskDelay(DelayTicks ? DelayTicks : 1); /* Minimum delay = 1 tick */     
        }    
    }
    
    goto exit;

    /* Print to stdout any data the server sends */
    ESP_LOGI(TAG,"Server:");
    ESP_LOGI(TAG, "buff here");

exit:    
    /* Cleanup and return */
    if (sockfd != SOCKET_INVALID) {
        close(sockfd); /* Close the connection to the server       */
    }
    
    if (ssl) {
        wolfSSL_free(ssl); /* Free the wolfSSL object                  */
    }
    
    if (ctx) {
        wolfSSL_CTX_free(ctx); /* Free the wolfSSL context object          */
    }
    
    wolfSSL_Cleanup(); /* Cleanup the wolfSSL environment          */

    return 0;                /* Return reporting a success               */
}


/** Event handler for Ethernet events */
static void eth_event_handler(void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    uint8_t mac_addr[6] = { 0 };
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
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
    
    //*ideally, we'd like to set time from network, but let's set a defaul time, just in case */
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
        const char* thisServer = ntpServer1[i];
        if (strncmp(thisServer,"\x00",1)) {
            /* just in case we run out of NTP servers */
            break;
        }
        sntp_setservername(i, thisServer);
    }
    sntp_init();
    return res;
}


void app_main(void)
{
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
    mac_config.smi_mdc_gpio_num = -1; // ENC28J60 doesn't have SMI interface
    mac_config.smi_mdio_gpio_num = -1;
    esp_eth_mac_t *mac = esp_eth_mac_new_enc28j60(&enc28j60_config, &mac_config);

    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.autonego_timeout_ms = 0; // ENC28J60 doesn't support auto-negotiation
    phy_config.reset_gpio_num = -1; // ENC28J60 doesn't have a pin to reset internal PHY
    esp_eth_phy_t *phy = esp_eth_phy_new_enc28j60(&phy_config);

    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&eth_config, &eth_handle));

    /* ENC28J60 doesn't burn any factory MAC address, we need to set it manually.
       02:00:00 is a Locally Administered OUI range so should not be used except when testing on a LAN under your control.
    */
    mac->set_addr(mac,
        (uint8_t[]) {
            0x02,
            0x00,
            0x00,
            0x12,
            0x34,
            0x56
        });

    /* attach Ethernet driver to TCP/IP stack */
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));

    /* Register user defined event handers 
     * "ensure that they register the user event handlers as the last thing prior to starting the Ethernet driver." 
    */
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));
    
    /* start Ethernet driver state machine */
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));
    
    // one of the most important aspects of security is the time and date values
    set_time();
    
    for (;;) {
        ESP_LOGI(TAG, "main loop");
        vTaskDelay(DelayTicks ? DelayTicks : 1); /* Minimum delay = 1 tick */     
        tls_smp_client_task();
    }
}

void app_main2(void)
{
    ESP_LOGI(TAG, "Start app_main...");
    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_LOGI(TAG, "Initialize wifi");
#if (ESP_IDF_VERSION_MAJOR >= 4 && ESP_IDF_VERSION_MINOR >= 1) || \
    (ESP_IDF_VERSION_MAJOR > 5)
    esp_netif_init();
#else
    tcpip_adapter_init();
#endif

    /* */
#if ESP_IDF_VERSION_MAJOR >= 4
    //(void) wifi_event_handler;
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
    * Read "Establishing Wi-Fi or Ethernet Connection" section in
    * examples/protocols/README.md for more information about this function.
    */
    // ESP_ERROR_CHECK(example_connect());
#else
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
        .ssid = TLS_SMP_WIFI_SSID,
        .password = TLS_SMP_WIFI_PASS,
    },
    };
    /* WiFi station mode */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    /* Wifi Set the configuration of the ESP32 STA or AP */ 
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    /* Start Wifi */
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    ESP_LOGI(TAG,
        "connect to ap SSID:%s password:%s",
        TLS_SMP_WIFI_SSID,
        TLS_SMP_WIFI_PASS);
#endif
    ESP_LOGI(TAG, "Set dummy time...");
   //set_time();
}