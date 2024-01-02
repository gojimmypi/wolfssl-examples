# wolfSSL TLS 1.3 ENC28J60 Server Example

This TLS1.3 example expands the [Espressif ENC28J60 Ethernet Example](https://github.com/espressif/esp-idf/tree/master/examples/ethernet/enc28j60)
to create a wolfSSL TLS1.3 TCP Server to a wolfSSL TLS1.3 TCP client to connect.

To open this solution in Visual Studio with the VisualGDB extension, see the [Solution File]()

With wolfSSL debugging turned on, sample serial output of the ESP32 can been found in the [example output](./README-output.txt)

# ENC28J60 Example
(See the README.md file in the upper level 'examples' directory for more information about examples.)

## Overview

ENC28J60 is a standalone Ethernet controller with a standard SPI interface. This example demonstrates how to drive this controller as an SPI device and then attach to TCP/IP stack.

This is also an example of how to integrate a new Ethernet MAC driver into the `esp_eth` component, without needing to modify the ESP-IDF component.

If you have a more complicated application to go (for example, connect to some IoT cloud via MQTT), you can always reuse the initialization codes in this example.

## How to use example

### Hardware Required

To run this example, you need to prepare following hardwares:
* [ESP32 board](https://docs.espressif.com/projects/esp-idf/en/latest/hw-reference/modules-and-boards.html) (e.g. ESP32-PICO, ESP32 DevKitC, etc)
* ENC28J60 module (the latest revision should be 6)
* **!! IMPORTANT !!** Proper input power source since ENC28J60 is quite power consuming device (it consumes more than 200 mA in peaks when transmitting). If improper power source is used, input voltage may drop and ENC28J60 may either provide nonsense response to host controller via SPI (fail to read registers properly) or it may enter to some strange state in the worst case. There are several options how to resolve it:
  * Power ESP32 board from `USB 3.0`, if board is used as source of power to ENC board.
  * Power ESP32 board from external 5V power supply with current limit at least 1 A, if board is used as source of power to ENC board.
  * Power ENC28J60 from external 3.3V power supply with common GND to ESP32 board. Note that there might be some ENC28J60 boards with integrated voltage regulator on market and so powered by 5 V. Please consult documentation of your board for details.

  If a ESP32 board is used as source of power to ENC board, ensure that that particular board is assembled with voltage regulator capable to deliver current up to 1 A. This is a case of ESP32 DevKitC or ESP-WROVER-KIT, for example. Such setup was tested and works as expected. Other boards may use different voltage regulators and may perform differently.
  **WARNING:** Always consult documentation/schematics associated with particular ENC28J60 and ESP32 boards used in your use-case first.

#### Pin Assignment

* ENC28J60 Ethernet module consumes one SPI interface plus an interrupt GPIO. By default they're connected as follows:

| GPIO   | ENC28J60    |
| ------ | ----------- |
| GPIO19 | SPI_CLK     |
| GPIO23 | SPI_MOSI    |
| GPIO25 | SPI_MISO    |
| GPIO22 | SPI_CS      |
| GPIO4  | Interrupt   |

*Important*: To make room for JTAG pins, the configuration *may* need to be manually set.
The unassigned defaults will _not_ work when also using a JTAG device, for example:

```text
CONFIG_EXAMPLE_ENC28J60_SCLK_GPIO=14
CONFIG_EXAMPLE_ENC28J60_MOSI_GPIO=13
CONFIG_EXAMPLE_ENC28J60_MISO_GPIO=12
CONFIG_EXAMPLE_ENC28J60_CS_GPIO=15
CONFIG_EXAMPLE_ENC28J60_SPI_CLOCK_MHZ=8
```

The [sdkconfig.defaults](./sdkconfig.defaults) _should_ assign pins like this:

```text
CONFIG_EXAMPLE_GPIO_RANGE_MIN=0
CONFIG_EXAMPLE_GPIO_RANGE_MAX=33
CONFIG_EXAMPLE_ENC28J60_SPI_HOST=1
CONFIG_EXAMPLE_ENC28J60_SCLK_GPIO=19
CONFIG_EXAMPLE_ENC28J60_MOSI_GPIO=23
CONFIG_EXAMPLE_ENC28J60_MISO_GPIO=25
CONFIG_EXAMPLE_ENC28J60_CS_GPIO=22
CONFIG_EXAMPLE_ENC28J60_SPI_CLOCK_MHZ=6
CONFIG_EXAMPLE_ENC28J60_INT_GPIO=4
# CONFIG_EXAMPLE_ENC28J60_DUPLEX_FULL is not set
CONFIG_EXAMPLE_ENC28J60_DUPLEX_HALF=y
```

Check the electrical connections and the `sdkconfig` settings if an error such as this 
is encountered at startup time:

```text
I (468) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (478) gpio: GPIO[4]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldow                                                 n: 0| Intr:0
I (488) enc28j60: revision: 0
E (488) enc28j60: enc28j60_verify_id(549): wrong chip ID
E (498) enc28j60: emac_enc28j60_init(1024): vefiry chip ID failed
I (508) gpio: GPIO[4]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldow                                                 n: 0| Intr:0
E (508) esp_eth: esp_eth_driver_install(214): init mac failed
ESP_ERROR_CHECK failed: esp_err_t 0xffffffff (ESP_FAIL) at 0x400861f8
file: "../../../main/enc28j60_example_main.c" line 1029
func: init_ENC28J60
expression: esp_eth_driver_install(&eth_config, &eth_handle)

abort() was called at PC 0x400861fb on core 0
```

### Configure the project

```
idf.py menuconfig
```

In the `Example Configuration` menu, set SPI specific configuration, such as SPI host number, 
GPIO used for MISO/MOSI/CS signal, GPIO for interrupt event and the SPI clock rate, duplex mode.
(see notes, above)

**Note:** According to ENC28J60 data sheet and our internal testing, SPI clock could reach up to 20MHz, 
but in practice, the clock speed may depend on your PCB layout/wiring/power source. In this example, 
the default clock rate is set to 8 MHz since some ENC28J60 silicon revisions may not properly work at 
frequencies less than 8 MHz.

### Build, Flash, and Run

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT -b 115200 build flash monitor
```

Replace PORT with the name of the serial port to use.

To exit the serial monitor, type ``Ctrl-]``.

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Example Output

```bash
I (0) cpu_start: Starting scheduler on APP CPU.
I (401) enc28j60: revision: 6
I (411) esp_eth.netif.glue: 00:04:a3:12:34:56
I (411) esp_eth.netif.glue: ethernet attached to netif
I (421) eth_example: Ethernet Started
I (2421) enc28j60: working in 10Mbps
I (2421) enc28j60: working in half duplex
I (2421) eth_example: Ethernet Link Up
I (2421) eth_example: Ethernet HW Addr 00:04:a3:12:34:56
I (4391) esp_netif_handlers: eth ip: 192.168.2.34, mask: 255.255.255.0, gw: 192.168.2.2
I (4391) eth_example: Ethernet Got IP Address
I (4391) eth_example: ~~~~~~~~~~~
I (4391) eth_example: ETHIP:192.168.2.34
I (4401) eth_example: ETHMASK:255.255.255.0
I (4401) eth_example: ETHGW:192.168.2.2
I (4411) eth_example: ~~~~~~~~~~~
```

Now you can ping your ESP32 in the terminal by entering `ping 192.168.2.34` (it depends on the actual IP address you get).

**Notes:**
1. ENC28J60 hasn't burned any valid MAC address in the chip, you need to write an unique MAC address into its internal MAC address register before any traffic happened on TX and RX line.
2. It is recommended to operate the ENC28J60 in full-duplex mode since various errata exist to the half-duplex mode (even though addressed in the example) and due to its poor performance in the half-duplex mode (especially in TCP connections). However, ENC28J60 does not support automatic duplex negotiation. If it is connected to an automatic duplex negotiation enabled network switch or Ethernet controller, then ENC28J60 will be detected as a half-duplex device. To communicate in Full-Duplex mode, ENC28J60 and the remote node (switch, router or Ethernet controller) **must be manually configured for full-duplex operation**:
   * The ENC28J60 can be set to full-duplex in the `Example Configuration` menu.
   * On Ubuntu/Debian Linux distribution use:
    ```
    sudo ethtool -s YOUR_INTERFACE_NAME speed 10 duplex full autoneg off
    ```
   * On Windows, go to `Network Connections` -> `Change adapter options` -> open `Properties` of selected network card -> `Configure` -> `Advanced` -> `Link Speed & Duplex` -> select `10 Mbps Full Duplex in dropdown menu`.
3. Ensure that your wiring between ESP32 board and the ENC28J60 board is realized by short wires with the same length and no wire crossings.
4. CS Hold Time needs to be configured to be at least 210 ns to properly read MAC and MII registers as defined by ENC28J60 Data Sheet. This is automatically configured in the example based on selected SPI clock frequency by computing amount of SPI bit-cycles the CS should stay active after the transmission. However, if your PCB design/wiring requires different value, please update `cs_ena_posttrans` member of `devcfg` structure per your actual needs.


## Troubleshooting

(For any technical queries, please open an [issue](https://github.com/espressif/esp-idf/issues) on GitHub. We will get back to you as soon as possible.)

<br />

## Support

Please contact wolfSSL at support@wolfssl.com with any questions, bug fixes,
or suggested feature additions.