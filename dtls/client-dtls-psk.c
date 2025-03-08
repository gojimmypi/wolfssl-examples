/* client-dtls-psk.c - DTLS Client using PSK
 *
 * Copyright (C) 2006-2025 wolfSSL Inc.
 *
 * This file is part of wolfSSL. (formerly known as CyaSSL)
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef WOLFSSL_USER_SETTINGS
#include <wolfssl/options.h>
#endif
#include <wolfssl/wolfcrypt/settings.h>
#include <wolfssl/ssl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

#define SERV_PORT   2256
#define MSGLEN      4096
#define DTLS_MTU    1500

static int cleanup = 0;
static void sig_handler(const int sig)
{
    printf("\nSIGINT %d handled\n", sig);
    cleanup = 1;
}

/* PSK identity and key */
static const char* psk_identity = "cyassl server";
static const unsigned char psk_key[] = { 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30 };
static const unsigned int psk_key_len = sizeof(psk_key);

/* PSK callback function */
static unsigned int psk_client_cb(WOLFSSL* ssl, const char* hint,
    char* identity, unsigned int id_max_len,
    unsigned char* key, unsigned int key_max_len)
{
    (void)ssl;
    (void)hint;

    printf("Hello PSK");

    if (strlen(psk_identity) + 1 > id_max_len || psk_key_len > key_max_len) {
        return 0;
    }
    strcpy(identity, psk_identity);
    memcpy(key, psk_key, psk_key_len);
    return psk_key_len;
}

int main(int argc, char** argv)
{
    int ret = 0, sockfd = -1;
    WOLFSSL* ssl = NULL;
    WOLFSSL_CTX* ctx = NULL;
    char buff[MSGLEN];
    int buffLen;
    struct sockaddr_in servAddr;

    if (argc != 2) {
        printf("usage: udpcli <IP address>\n");
        return 1;
    }

    struct sigaction act;
    act.sa_handler = sig_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGINT, &act, NULL);

    wolfSSL_Debugging_ON();
    wolfSSL_Init();

    if ((ctx = wolfSSL_CTX_new(wolfDTLSv1_2_client_method())) == NULL) {
        fprintf(stderr, "wolfSSL_CTX_new error.\n");
        goto exit;
    }

    /* Set PSK client callback */
    printf("Setting PSK client callback...\n");
    wolfSSL_CTX_set_psk_client_callback(ctx, psk_client_cb);
    printf("wolfSSL_CTX_set_psk_client_callback result = %d\n", ret);


    char cipherlist[4096];
    wolfSSL_get_ciphers(cipherlist, sizeof(cipherlist));
    printf("Client Available Ciphers: %s\n", cipherlist);

    //ret = wolfSSL_CTX_set_cipher_list(ctx, "DHE-PSK-AES128-CBC-SHA256");
    ret = wolfSSL_CTX_set_cipher_list(ctx, "TLS_DHE_PSK_WITH_AES_128_CBC_SHA256");

    if (ret == SSL_SUCCESS) {
        printf("Success: Set cipher TLS_DHE_PSK_WITH_AES_128_CBC_SHA256\n");
    }
    else {
        printf("Error setting cipher: %d\n", ret);
    }

    if (wolfSSL_CTX_use_psk_identity_hint(ctx, "wolfssl-client") != SSL_SUCCESS) {
        printf("Failed to set PSK identity hint\n");
    }
    else {
        printf("PSK identity hint set successfully\n");
    }


    ssl = wolfSSL_new(ctx);
    if (ssl == NULL) {
        fprintf(stderr, "unable to get ssl object\n");
        goto exit;
    }

    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(SERV_PORT);
    printf("Port %d\n", SERV_PORT);
    if (inet_pton(AF_INET, argv[1], &servAddr.sin_addr) < 1) {
        printf("Error and/or invalid IP address\n");
        goto exit;
    }

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("cannot create a socket.\n");
        goto exit;
    }

    /* Connect UDP socket to server */
    if (connect(sockfd, (struct sockaddr*)&servAddr, sizeof(servAddr)) < 0) {
        perror("Failed to connect UDP socket to server");
        goto exit;
    }
    printf("UDP socket connected to server\n");

    wolfSSL_set_fd(ssl, sockfd);

    //struct sockaddr_in clientAddr;
    //memset(&clientAddr, 0, sizeof(clientAddr));
    //clientAddr.sin_family = AF_INET;
    //clientAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    //clientAddr.sin_port = htons(0);  // Let OS pick a free port

    //if (bind(sockfd, (struct sockaddr*)&clientAddr, sizeof(clientAddr)) < 0) {
    //    perror("Failed to bind client socket");
    //    goto exit;
    //}
    //printf("Client socket bound successfully\n");



    ret = wolfSSL_connect(ssl);
    int err = wolfSSL_get_error(ssl, 0);
    printf("SSL_connect failed: err = %d, %s\n", err, wolfSSL_ERR_reason_error_string(err));

    if (ret != SSL_SUCCESS) {
        printf("SSL_connect failed!\n");
        goto exit;
    }

    if (fgets(buff, sizeof(buff), stdin) != NULL) {
        buffLen = strlen(buff);
        if ((wolfSSL_write(ssl, buff, buffLen)) != buffLen) {
            printf("SSL_write failed\n");
            goto exit;
        }
        ret = wolfSSL_read(ssl, buff, sizeof(buff) - 1);
        if (ret < 0) {
            printf("SSL_read failed\n");
            goto exit;
        }
        buff[ret] = '\0';
        fputs(buff, stdout);
    }

exit:
    if (ssl) {
        wolfSSL_shutdown(ssl);
        wolfSSL_free(ssl);
    }
    if (sockfd != -1) {
        close(sockfd);
    }
    if (ctx) {
        wolfSSL_CTX_free(ctx);
    }
    wolfSSL_Cleanup();
    return ret;
}
