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

#define DEFAULT_PORT                     11111

#define TLS_SMP_CLIENT_TASK_NAME         "tls_server_example"
#define TLS_SMP_CLIENT_TASK_WORDS        10240
#define TLS_SMP_CLIENT_TASK_PRIORITY     8



/* include certificates. Note that there is an experiation date! 
 * 
 * See also https://github.com/wolfSSL/wolfssl/blob/master/wolfssl/certs_test.h
 
   for example:
     
    #define USE_CERT_BUFFERS_2048
    #include <wolfssl/certs_test.h>
*/

/* 
* for reference, client uses:
* 
* #include "embedded_CA_FILE.h"
* #include "embedded_CERT_FILE.h"
* #include "embedded_KEY_FILE.h"
*/

/*
server file versions:
#define CA_FILE   "../../../../certs/client-cert.pem"
#define CERT_FILE "../../../../certs/server-cert.pem"
#define KEY_FILE  "../../../../certs/server-key.pem"
*/

#include "embedded_CLIENT_CERT_FILE.h"
#include "embedded_SERVER_CERT_FILE.h"
#include "embedded_SERVER_KEY_FILE.h"


static const char *TAG = "eth_example";

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

/* const char sndMsg[] = "GET /index.html HTTP/1.0\r\n\r\n"; */
const char sendMessage[] = "Hello World\n";
const int sendMessageSize = sizeof(sendMessage);

TickType_t DelayTicks = 5000 / portTICK_PERIOD_MS;
/**
 ******************************************************************************
 ******************************************************************************
 ** USER SETTINGS END
 ******************************************************************************
 ******************************************************************************
 **/
  


#ifdef HAVE_SIGNAL
static void sig_handler(const int sig) {
//    fprintf(stderr, "SIGINT handled = %d.\n", sig);

    mShutdown = 1;
    if (mConnd != SOCKET_INVALID) {
        close(mConnd); /* Close the connection to the client   */
        mConnd = SOCKET_INVALID;
    }
    if (mSockfd != SOCKET_INVALID) {
        close(mSockfd); /* Close the socket listening for clients   */
        mSockfd = SOCKET_INVALID;
    }
}
#endif

int tls_smp_client_task() {
    int ret = WOLFSSL_SUCCESS; /* assume success until proven wrong */
    int sockfd = 0; /* the socket that will carry our secure connection */
    struct sockaddr_in servAddr;
    const int BUFF_SIZE = 256;
    char buff[BUFF_SIZE];
    size_t len; /* we'll be looking at the length of messages sent and received */

    struct sockaddr_in clientAddr;
    socklen_t          size = sizeof(clientAddr);
    const char*        reply = "I hear ya fa shizzle!\n";
    int                on;

    static int mConnd = SOCKET_INVALID;
    static int mShutdown = 0;


#ifdef WOLFSSL_TLS13

    /* declare wolfSSL objects */
    WOLFSSL_CTX *ctx = NULL; /* the wolfSSL context object*/
    WOLFSSL *ssl = NULL; /* although called "ssl" is is the secure object for reading and writings data*/

#ifdef HAVE_SIGNAL
    signal(SIGINT, sig_handler);
#endif

#ifdef DEBUG_WOLFSSL
    wolfSSL_Debugging_ON();
    WOLFSSL_MSG("Debug ON v0.2b");
    //ShowCiphers();
#endif
    
    
    /* Initialize the server address struct with zeros */
    memset(&servAddr, 0, sizeof(servAddr));

    /* Fill in the server address */
    servAddr.sin_family      = AF_INET; /* using IPv4      */
    servAddr.sin_port        = htons(DEFAULT_PORT); /* on DEFAULT_PORT */
    servAddr.sin_addr.s_addr = INADDR_ANY; /* from anywhere   */

    /* 
    ***************************************************************************
    * Create a socket that uses an internet IPv4 address,
    * Sets the socket to be stream based (TCP),
    * 0 means choose the default protocol.
    * 
    *  #include <sys/socket.h>
    *
    *  int socket(int domain, int type, int protocol);  
    *  
    *  see: https://linux.die.net/man/3/socket
    ***************************************************************************
    */
    if (ret == WOLFSSL_SUCCESS) {
        /* Upon successful completion, socket() shall return 
         * a non-negative integer, the socket file descriptor.
        */
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd > 0) {
            WOLFSSL_MSG("socket creation successful\n");
        }
        else {
            // TODO show errno 
            ret = WOLFSSL_FAILURE;
            WOLFSSL_ERROR_MSG("ERROR: failed to create a socket.\n");
        }
    }
    else {
        WOLFSSL_ERROR_MSG("Skipping socket create.\n");
    }


    /*
    ***************************************************************************
    * set SO_REUSEADDR on socket
    * 
    *  #include <sys/types.h>
    *  # include <sys / socket.h>
    *  int getsockopt(int sockfd,
    *    int level,
    *    int optname,
    *    void *optval,
    *    socklen_t *optlen); int setsockopt(int sockfd,
    *    int level,
    *    int optname,
    *    const void *optval,
    *    socklen_t optlen);
    *    
    *  setsockopt() manipulates options for the socket referred to by the file 
    *  descriptor sockfd. Options may exist at multiple protocol levels; they 
    *  are always present at the uppermost socket level.
    *  
    *  When manipulating socket options, the level at which the option resides 
    *  and the name of the option must be specified. To manipulate options at 
    *  the sockets API level, level is specified as SOL_SOCKET. To manipulate 
    *  options at any other level the protocol number of the appropriate 
    *  protocol controlling the option is supplied. For example, to indicate 
    *  that an option is to be interpreted by the TCP protocol, level should 
    *  be set to the protocol number of TCP
    *  
    *  Return Value
    *    On success, zero is returned. On error, -1 is returned, and errno is set appropriately.
    *
    *  Errors
    *    EBADF       The argument sockfd is not a valid descriptor.
    *    EFAULT      The address pointed to by optval is not in a valid part of the process address space. For getsockopt(), this error may also be returned if optlen is not in a valid part of the process address space.
    *    EINVAL      optlen invalid in setsockopt(). In some cases this error can also occur for an invalid value in optval (e.g., for the IP_ADD_MEMBERSHIP option described in ip(7)).
    *    ENOPROTOOPT The option is unknown at the level indicated.
    *    ENOTSOCK    The argument sockfd is a file, not a socket.
    *
    *  see: https://linux.die.net/man/2/setsockopt
    ***************************************************************************
    */
    if (ret == WOLFSSL_SUCCESS) {
        /* make sure server is setup for reuse addr/port */
        on = 1;
        int soc_ret = setsockopt(sockfd,
            SOL_SOCKET,
            SO_REUSEADDR,
            (char*)&on,
            (socklen_t)sizeof(on));
        
        if (soc_ret == 0) {
            WOLFSSL_MSG("setsockopt re-use addr successful\n");
        }
        else {
            // TODO show errno 
            ret = WOLFSSL_FAILURE;
            WOLFSSL_ERROR_MSG("ERROR: failed to setsockopt addr on socket.\n");
        }
    }
    else {
        WOLFSSL_ERROR_MSG("Skipping setsockopt addr\n");
    }
        
#ifdef SO_REUSEPORT
    /* see above for details on getsockopt  */
    if (ret == WOLFSSL_SUCCESS) {
        int soc_ret = setsockopt(sockfd,
            SOL_SOCKET,
            SO_REUSEPORT,
            (char*)&on,
            (socklen_t)sizeof(on));
            
        if (soc_ret == 0) {
            WOLFSSL_MSG("setsockopt re-use port successful\n");
        }
        else {
            // TODO show errno 
            // ret = WOLFSSL_FAILURE;
            // TODO what's up with the error?
            WOLFSSL_ERROR_MSG("ERROR: failed to setsockopt port on socket.  >> IGNORED << \n");
        }
    } 
    else {
        WOLFSSL_ERROR_MSG("Skipping setsockopt port\n");
    }
#else
    WOLFSSL_MSG("SO_REUSEPORT not configured for setsockopt to re-use port\n");
#endif
    
    /*
    ***************************************************************************
    *  #include <sys/types.h>  
    *  #include <sys/socket.h>
    *  
    *  int bind(int sockfd,
    *      const struct sockaddr *addr,
    *      socklen_t addrlen);
    *      
    *  Description
    *  
    *  When a socket is created with socket(2), it exists in a name 
    *  space(address family) but has no address assigned to it.
    * 
    *  bind() assigns the address specified by addr to the socket referred to 
    *  by the file descriptor sockfd.addrlen specifies the size, in bytes, of 
    *  the address structure pointed to by addr.Traditionally, this operation 
    *  is called "assigning a name to a socket".
    *  
    *   It is normally necessary to assign a local address using bind() before
    *   a SOCK_STREAM socket may receive connections.
    *
    *  Return Value
    *    On success, zero is returned. On error, -1 is returned, and errno is set appropriately.
    *
    *  Errors
    *    EACCES     The address is protected, and the user is not the superuser.
    *    EADDRINUSE The given address is already in use.
    *    EBADF      sockfd is not a valid descriptor.
    *    EINVAL     The socket is already bound to an address.
    *    ENOTSOCK   sockfd is a descriptor for a file, not a socket.
    *
    *   see: https://linux.die.net/man/2/bind
    *   
    *       https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/lwip.html
    ***************************************************************************
    */
    if (ret == WOLFSSL_SUCCESS) {
        /* Bind the server socket to our port */
        int soc_ret = bind(sockfd, (struct sockaddr*)&servAddr, sizeof(servAddr));
        if (soc_ret > -1) {
            WOLFSSL_MSG("socket bind successful\n");
        }
        else {
            ret = WOLFSSL_FAILURE;
            WOLFSSL_ERROR_MSG("ERROR: failed to bind to socket.\n");
        }
    }

    /* 
    ***************************************************************************
    *  Listen for a new connection, allow 5 pending connections 
    *
    *  #include <sys/types.h>  
    *  #include <sys/socket.h>
    *  int listen(int sockfd, int backlog);
    *
    *  Description
    *  
    *  listen() marks the socket referred to by sockfd as a passive socket, 
    *  that is, as a socket that will be used to accept incoming connection 
    *  requests using accept.
    *
    *  The sockfd argument is a file descriptor that refers to a socket of 
    *  type SOCK_STREAM or SOCK_SEQPACKET.
    *  
    *  The backlog argument defines the maximum length to which the queue of 
    *  pending connections for sockfd may grow.If a connection request arrives 
    *  when the queue is full, the client may receive an error with an indication
    *  of ECONNREFUSED or, if the underlying protocol supports retransmission, 
    *  the request may be ignored so that a later reattempt at connection 
    *  succeeds.
    *
    *   Return Value
    *     On success, zero is returned.
    *     On Error, -1 is returned, and errno is set appropriately.
    *   Errors  
    *     EADDRINUSE   Another socket is already listening on the same port.
    *     EBADF        The argument sockfd is not a valid descriptor.
    *     ENOTSOCK     The argument sockfd is not a socket.
    *     EOPNOTSUPP   The socket is not of a type that supports the listen() operation.
    *
    *  ses: https://linux.die.net/man/2/listen
    */
    
    if(ret == WOLFSSL_SUCCESS) {
        int soc_ret = listen(sockfd, 5);
        if (soc_ret > -1) {
            WOLFSSL_MSG("socket listen successful\n");
        }
        else {
            ret = WOLFSSL_FAILURE;
            WOLFSSL_ERROR_MSG("ERROR: failed to listen to socket.\n");
        }
    }    
    
    /* 
    ***************************************************************************
    * Initialize wolfSSL 
    * 
    *  WOLFSSL_API int wolfSSL_Init    (void)
    *
    *  Initializes the wolfSSL library for use. Must be called once per 
    *  application and before any other call to the library.
    *
    *  Returns
    *    SSL_SUCCESS  If successful the call will return.
    *    BAD_MUTEX_E  is an error that may be returned.
    *    WC_INIT_E    wolfCrypt initialization error returned.
    * 
    *  see: https://www.wolfssl.com/doxygen/group__TLS.html#gae2a25854de5230820a6edf16281d8fd7
    ***************************************************************************
    */
    if (ret == WOLFSSL_SUCCESS) {
        /* only proceed if the prior step was successful */
        WOLFSSL_MSG("calling wolfSSL_Init");
        ret = wolfSSL_Init();

        if (ret == WOLFSSL_SUCCESS) {
            WOLFSSL_MSG("wolfSSL_Init successful\n");
        }
        else {
            WOLFSSL_ERROR_MSG("ERROR: wolfSSL_Init failed\n");
        }
    }
    else {
        WOLFSSL_ERROR_MSG("Skipping wolfSSL_Init\n");
    }

    /* 
    ***************************************************************************
    * Create and initialize WOLFSSL_CTX (aka the context)
    * 
    *  WOLFSSL_API WOLFSSL_CTX* wolfSSL_CTX_new    (WOLFSSL_METHOD *)
    * 
    *  This function creates a new SSL context, taking a desired 
    *  SSL/TLS protocol method for input.
    *
    *  Returns
    *    pointer If successful the call will return a pointer to the newly-created WOLFSSL_CTX.
    *    NULL upon failure.
    *
    *  Parameters
    *    method pointer to the desired WOLFSSL_METHOD to use for the SSL context. 
    *    This is created using one of the wolfSSLvXX_XXXX_method() functions to
    *    specify SSL/TLS/DTLS protocol level.
    * 
    *  see https://www.wolfssl.com/doxygen/group__Setup.html#gadfa552e771944a6a1102aa43f45378b5
    ***************************************************************************
    */
    if (ret == WOLFSSL_SUCCESS) {
        WOLFSSL_METHOD* method = wolfTLSv1_3_server_method();
        WOLFSSL_MSG("calling wolfTLSv1_3_server_method");
        if (method == NULL) {
            WOLFSSL_ERROR_MSG("ERROR : failed to get wolfTLSv1_3_server_method.\n");
            ret = WOLFSSL_FAILURE;
        }
        else {
            WOLFSSL_MSG("calling wolfSSL_CTX_new");
                ctx = wolfSSL_CTX_new(method);

            if (ctx == NULL) {
                WOLFSSL_ERROR_MSG("ERROR : failed to create WOLFSSL_CTX\n");
                ret = WOLFSSL_FAILURE;
            }
        }
    }
    else {
        WOLFSSL_ERROR_MSG("skipping wolfSSL_CTX_new\n");
    }

    /* 
    ***************************************************************************
    *  load CERT_FILE 
    *  
    *  
    *  WOLFSSL_API int wolfSSL_use_certificate_buffer (WOLFSSL * ,
    *                                                  const unsigned char * ,
    *                                                  long,
    *                                                  int      
    *                                                  )
    *  
    *  The wolfSSL_use_certificate_buffer() function loads a certificate buffer 
    *  into the WOLFSSL object. It behaves like the non-buffered version, only 
    *  differing in its ability to be called with a buffer as input instead of 
    *  a file. The buffer is provided by the in argument of size sz. 
    *  
    *  format specifies the format type of the buffer; SSL_FILETYPE_ASN1 or 
    *  SSL_FILETYPE_PEM. Please see the examples for proper usage.
    *  
    *  Returns
    *    SSL_SUCCESS      upon success.
    *    SSL_BAD_FILETYPE will be returned if the file is the wrong format.
    *    SSL_BAD_FILE     will be returned if the file doesn’t exist, can’t be read, or is corrupted.
    *    MEMORY_E         will be returned if an out of memory condition occurs.
    *    ASN_INPUT_E      will be returned if Base16 decoding fails on the file.
    *  
    *  Parameters
    *    ssl    pointer to the SSL session, created with wolfSSL_new().
    *    in     buffer containing certificate to load.
    *    sz     size of the certificate located in buffer.
    *    format format of the certificate to be loaded. Possible values are SSL_FILETYPE_ASN1 or SSL_FILETYPE_PEM.
    *  
    *
    *  Pay attention to expiration dates and the current date setting
    *  
    *  see https://www.wolfssl.com/doxygen/group__CertsKeys.html#gaf4e8d912f3fe2c37731863e1cad5c97e
    ***************************************************************************
    */
    if (ret == WOLFSSL_SUCCESS) {
        WOLFSSL_MSG("Loading cert");
        ret = wolfSSL_CTX_use_certificate_buffer(ctx, 
            CERT_FILE, 
            sizeof_CERT_FILE(), 
            WOLFSSL_FILETYPE_PEM);

        if (ret == WOLFSSL_SUCCESS) {
            WOLFSSL_MSG("wolfSSL_CTX_use_certificate_buffer successful\n");
        }
        else {
            WOLFSSL_ERROR_MSG("ERROR: wolfSSL_CTX_use_certificate_buffer failed\n");
        }
    }
    else {
        WOLFSSL_ERROR_MSG("skipping wolfSSL_CTX_use_certificate_buffer\n");
    }
        
    
    /* 
    ***************************************************************************
    *  Load client private key into WOLFSSL_CTX 
    *  
    *  wolfSSL_CTX_use_PrivateKey_buffer()
    *  
    *  WOLFSSL_API int wolfSSL_CTX_use_PrivateKey_buffer(WOLFSSL_CTX *,
    *                                                    const unsigned char *,
    *                                                    long,
    *                                                    int      
    *                                                   )
    *
    *  This function loads a private key buffer into the SSL Context. 
    *  It behaves like the non-buffered version, only differing in its 
    *  ability to be called with a buffer as input instead of a file. 
    *  
    *  The buffer is provided by the in argument of size sz. format 
    *  specifies the format type of the buffer; 
    *  SSL_FILETYPE_ASN1 or SSL_FILETYPE_PEM. 
    *  
    *  Please see the examples for proper usage.
    *
    *  Returns
    *    SSL_SUCCESS upon success
    *    SSL_BAD_FILETYPE will be returned if the file is the wrong format.
    *    SSL_BAD_FILE will be returned if the file doesn’t exist, can’t be read, or is corrupted.
    *    MEMORY_E will be returned if an out of memory condition occurs.
    *    ASN_INPUT_E will be returned if Base16 decoding fails on the file.
    *    NO_PASSWORD will be returned if the key file is encrypted but no password is provided.
    *
    *  Parameters
    *    ctx      pointer to the SSL context, created with wolfSSL_CTX_new().
    *             inthe input buffer containing the private key to be loaded.
    *    
    *    sz          the size of the input buffer.
    *    
    *    format  the format of the private key located in the input buffer(in). 
    *            Possible values are SSL_FILETYPE_ASN1 or SSL_FILETYPE_PEM.
    *
    *  see: https://www.wolfssl.com/doxygen/group__CertsKeys.html#ga71850887b87138b7c2d794bf6b1eafab
    ***************************************************************************
    */
    if (ret == WOLFSSL_SUCCESS) {
        ret = wolfSSL_CTX_use_PrivateKey_buffer(ctx, 
                                                KEY_FILE, 
                                                sizeof_KEY_FILE(), 
                                                WOLFSSL_FILETYPE_PEM);
        if (ret == WOLFSSL_SUCCESS) {
            WOLFSSL_MSG("wolfSSL_CTX_use_PrivateKey_buffer successful\n");
        }
        else {
            /* TODO fetch and print expiration date since it is a common fail */
            WOLFSSL_ERROR_MSG("ERROR: wolfSSL_CTX_use_PrivateKey_buffer failed\n");
        }
    }
    else {
        WOLFSSL_ERROR_MSG("Skipping wolfSSL_CTX_use_PrivateKey_buffer\n");
    }

    
    /* 
    ***************************************************************************
    *  Load CA certificate into WOLFSSL_CTX 
    * 
    *  wolfSSL_CTX_load_verify_buffer()
    *  WOLFSSL_API int wolfSSL_CTX_load_verify_buffer(WOLFSSL_CTX *,
    *                                                 const unsigned char *,
    *                                                 long,
    *                                                 int      
    *                                                )        
    *                                                
    *  This function loads a CA certificate buffer into the WOLFSSL Context. 
    *  It behaves like the non-buffered version, only differing in its ability 
    *  to be called with a buffer as input instead of a file. The buffer is 
    *  provided by the in argument of size sz. format specifies the format type 
    *  of the buffer; SSL_FILETYPE_ASN1 or SSL_FILETYPE_PEM. More than one 
    *  CA certificate may be loaded per buffer as long as the format is in PEM.
    *  
    *  Please see the examples for proper usage.
    *  
    *  Returns
    *  
    *    SSL_SUCCESS upon success
    *    SSL_BAD_FILETYPE will be returned if the file is the wrong format.
    *    SSL_BAD_FILE will be returned if the file doesn’t exist, can’t be read, or is corrupted.
    *    MEMORY_E will be returned if an out of memory condition occurs.
    *    ASN_INPUT_E will be returned if Base16 decoding fails on the file.
    *    BUFFER_E will be returned if a chain buffer is bigger than the receiving buffer.
    *    
    *  Parameters
    *  
    *    ctx    pointer to the SSL context, created with wolfSSL_CTX_new().
    *    in    pointer to the CA certificate buffer.
    *    sz    size of the input CA certificate buffer, in.
    *    format    format of the buffer certificate, either SSL_FILETYPE_ASN1 or SSL_FILETYPE_PEM.
    *
    * see https://www.wolfssl.com/doxygen/group__CertsKeys.html#gaa37539cce3388c628ac4672cf5606785
    ***************************************************************************
    */
    if (ret == WOLFSSL_SUCCESS) {
        ret = wolfSSL_CTX_load_verify_buffer(ctx, CA_FILE, sizeof_CA_FILE(), WOLFSSL_FILETYPE_PEM);
        if (ret == WOLFSSL_SUCCESS) {
            WOLFSSL_MSG("wolfSSL_CTX_load_verify_buffer successful\n");
        }
        else {
            WOLFSSL_ERROR_MSG("ERROR: wolfSSL_CTX_load_verify_buffer failed\n");
        }
    }
    else {
        WOLFSSL_ERROR_MSG("skipping wolfSSL_CTX_load_verify_buffer\n");
    }
 
    /* Require mutual authentication */
    wolfSSL_CTX_set_verify(ctx,
        WOLFSSL_VERIFY_PEER | WOLFSSL_VERIFY_FAIL_IF_NO_PEER_CERT,
        NULL);

    

    
    /* Continue to accept clients until mShutdown is issued */
    while (!mShutdown && (ret == WOLFSSL_SUCCESS)) {
        WOLFSSL_MSG("Waiting for a connection...\n");
        
        /* Accept client connections */
        if ((mConnd = accept(sockfd, (struct sockaddr*)&clientAddr, &size))
            == -1) {
            // fprintf(stderr, "ERROR: failed to accept the connection\n\n");
            ret = -1; 
            // TODO    goto exit;
                WOLFSSL_ERROR_MSG("ERROR: failed socket accept\n");
                ret = WOLFSSL_FAILURE;
        }

        /* Create a WOLFSSL object */
        if ((ssl = wolfSSL_new(ctx)) == NULL) {
            // fprintf(stderr, "ERROR: failed to create WOLFSSL object\n");
            ret = -1; 
            //TODO goto exit;
            WOLFSSL_ERROR_MSG("ERROR: filed wolfSSL_new during loop\n");
            ret = WOLFSSL_FAILURE;
    }

        /* Attach wolfSSL to the socket */
        wolfSSL_set_fd(ssl, mConnd);

#ifdef HAVE_SECRET_CALLBACK
        /* required for getting random used */
        wolfSSL_KeepArrays(ssl);

        /* optional logging for wireshark */
        wolfSSL_set_tls13_secret_cb(ssl,
            Tls13SecretCallback,
            (void*)WOLFSSL_SSLKEYLOGFILE_OUTPUT);
#endif

        /* Establish TLS connection */
        if ((ret = wolfSSL_accept(ssl)) != WOLFSSL_SUCCESS) {
            WOLFSSL_ERROR_MSG("ERROR: wolfSSL_accept\n");
            ret = WOLFSSL_FAILURE;
            // fprintf(stderr,
            //       "wolfSSL_accept error = %d\n",
            //    wolfSSL_get_error(ssl, ret));
            // TODO goto exit;
        }
        else {
            WOLFSSL_MSG("Client connected successfully\n");
        }


#ifdef HAVE_SECRET_CALLBACK
        wolfSSL_FreeArrays(ssl);
#endif

        /* Read the client data into our buff array */
        memset(buff, 0, sizeof(buff));
        if ((ret = wolfSSL_read(ssl, buff, sizeof(buff) - 1)) < 0) {
            // fprintf(stderr, "ERROR: failed to read\n");
            //TODO goto exit;
        }

        /* Print to stdout any data the client sends */
        // printf("Client: %s\n", buff);

        /* Check for server shutdown command */
        if (strncmp(buff, "shutdown", 8) == 0) {
            // printf("Shutdown command issued!\n");
            mShutdown = 1;
        }

        /* Write our reply into buff */
        memset(buff, 0, sizeof(buff));
        memcpy(buff, reply, strlen(reply));
        len = strnlen(buff, sizeof(buff));

        /* Reply back to the client */
        if ((ret = wolfSSL_write(ssl, buff, len)) != len) {
            // fprintf(stderr, "ERROR: failed to write\n");
            // TODO goto exit;
        }

        /* Cleanup after this connection */
        wolfSSL_shutdown(ssl);
        if (ssl) {
            wolfSSL_free(ssl); /* Free the wolfSSL object              */
            ssl = NULL;
        }
        if (mConnd != SOCKET_INVALID) {
            close(mConnd); /* Close the connection to the client   */
            mConnd = SOCKET_INVALID;
        }
    }

    WOLFSSL_MSG("Shutdown complete\n");

    /* 
    ***************************************************************************
    *    
    *    Cleanup and return 
    *    
    ***************************************************************************
    */
    if (mConnd != SOCKET_INVALID) {
        close(mConnd); /* Close the connection to the client   */
        mConnd = SOCKET_INVALID;
    }

    if (sockfd != SOCKET_INVALID) {
        close(sockfd); /* Close the socket listening for clients   */
        sockfd = SOCKET_INVALID;
    }
    
    if (ssl) {
        wolfSSL_free(ssl); /* Free the wolfSSL object */
    }

    
    if (ctx) {
        wolfSSL_CTX_free(ctx); /* Free the wolfSSL context object          */
    }
    
    wolfSSL_Cleanup(); /* Cleanup the wolfSSL environment          */

#else
    printf("Example requires TLS v1.3\n");
#endif /* WOLFSSL_TLS13 */
    return ret;
}


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
        tls_smp_client_task();
    }
}
