# wolfSSL ESP32 Examples

## ESP32 TLS 1.3 Server

- [Wireless STA server](./TLS13-wifi_station-server/README.md)
- [Wired ENC28J60 server](./TLS13-ENC28J60-server/README.md)

coming soon:
- Wireless AP client
- Arduino client

## ESP32 TLS 1.3 Client

- [Wireless STA client](./TLS13-wifi_station-client/README.md)
- [Wired ENC28J60 client](./TLS13-ENC28J60-client/README.md)

coming soon:
- Wireless AP server
- Arduino server

<br />
# Tips

If JTAG gets into a mode where it is simply always returning an error (app continually resetting)
try using serial port to program a basic, operational ["hello world"](./ESP32-hello-world/README.md). 
The Arduino IDE or command-line ESP-IDF can be handy here.


## See also:

- [wolfSSL](https://www.wolfssl.com/)
- [Espressif](https://www.espressif.com/)

- [wolfSSL Quick Start Guide](https://www.wolfssl.com/docs/quickstart/)
- [github.com/wolfSSL/wolfssl](https://github.com/wolfSSL/wolfssl)
- [wolfSSL blog: ESP32 Hardware Acceleration Support](https://www.wolfssl.com/wolfssl-esp32-hardware-acceleration-support/)
- [wolfSSL blog: Support for ESP-IDF and ESP32-WROOM-32](https://www.wolfssl.com/wolfssl-support-esp-idf-esp32-wroom-32/)
- [wolfSSL Context and Session Set Up](https://www.wolfssl.com/doxygen/group__Setup.html)
- [wolfSSL Certificates and Keys](https://www.wolfssl.com/doxygen/group__CertsKeys.html)
- [wolfSSL Error Handling and Reporting](https://www.wolfssl.com/doxygen/group__Debug.html)
- [wolfSSL Espressif Support Docs](https://www.wolfssl.com/docs/espressif/)
- [wolfSSL Licensing](https://www.wolfssl.com/license/)
- [wolfssl/IDE/Espressif/ESP-IDF](https://github.com/wolfSSL/wolfssl/tree/master/IDE/Espressif/ESP-IDF) (will be migrating to [wolfSSL/wolfssl-examples](https://github.com/wolfSSL/wolfssl-examples/) )
- [wolfssl/error-ssl.h](https://github.com/wolfSSL/wolfssl/blob/master/wolfssl/error-ssl.h)
- [wolfssl/certs_test.h](https://github.com/wolfSSL/wolfssl/blob/master/wolfssl/certs_test.h)
- [Espressif ESP32 Series of Modules](https://www.espressif.com/en/products/modules/esp32)
- [Espressif ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/index.html)
- [Espressif System Time API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/system_time.html)
- [Espressif Networking API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_eth.html)
- [linux socket](https://linux.die.net/man/3/socket)
- [linux socket connect](https://linux.die.net/man/3/connect)
- [stackoverflow: What are the differences between .pem, .cer and .der?](https://stackoverflow.com/questions/22743415/what-are-the-differences-between-pem-cer-and-der)
- [esp32.com: How to check cip version](https://www.esp32.com/viewtopic.php?t=16103)
- [VisualGDB Extension for Visual Studio](https://visualgdb.com/)
- [Wireshark](https://www.wireshark.org/)
- [WSL](https://docs.microsoft.com/en-us/windows/wsl/)