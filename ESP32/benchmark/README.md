# Benchmark

ESP32-C3 RISC-V (no hardware accleration)

```
Benchmark Test
------------------------------------------------------------------------------
 wolfSSL version 5.4.0
------------------------------------------------------------------------------
wolfCrypt Benchmark (block bytes 1024, min 1.0 sec each)
RNG                  6 MB took 1.002 seconds,    6.286 MB/s
AES-128-CBC-enc     11 MB took 1.000 seconds,   10.986 MB/s
AES-128-CBC-dec     10 MB took 1.000 seconds,   10.059 MB/s
AES-192-CBC-enc     10 MB took 1.000 seconds,    9.888 MB/s
AES-192-CBC-dec      9 MB took 1.002 seconds,    9.137 MB/s
AES-256-CBC-enc      9 MB took 1.000 seconds,    9.009 MB/s
AES-256-CBC-dec      8 MB took 1.002 seconds,    8.382 MB/s
AES-128-GCM-enc      3 MB took 1.005 seconds,    2.551 MB/s
AES-128-GCM-dec      3 MB took 1.005 seconds,    2.551 MB/s
AES-192-GCM-enc      2 MB took 1.002 seconds,    2.485 MB/s
AES-192-GCM-dec      2 MB took 1.003 seconds,    2.483 MB/s
AES-256-GCM-enc      2 MB took 1.006 seconds,    2.427 MB/s
AES-256-GCM-dec      2 MB took 1.007 seconds,    2.424 MB/s
GMAC Default         3 MB took 1.000 seconds,    3.348 MB/s
3DES                 3 MB took 1.001 seconds,    3.171 MB/s
MD5                103 MB took 1.001 seconds,  102.510 MB/s
SHA                 49 MB took 1.000 seconds,   48.828 MB/s
SHA-256             15 MB took 1.000 seconds,   14.844 MB/s
SHA-512             13 MB took 1.001 seconds,   12.927 MB/s
```

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This example demonstrates how to blink a LED using GPIO or RMT for the addressable LED, i.e. [WS2812](http://www.world-semi.com/Certifications/WS2812B.html).

See the RMT examples in the [RMT Peripheral](../../peripherals/rmt) for more information about how to use it.

## How to Use Example

Before project configuration and build, be sure to set the correct chip target using `idf.py set-target <chip_name>`.

### Hardware Required

* A development board with ESP32/ESP32-S2/ESP32-S3/ESP32-C3 SoC (e.g., ESP32-DevKitC, ESP-WROVER-KIT, etc.)
* A USB cable for Power supply and programming

Some development boards use an addressable LED instead of a regular one. These development boards include:

| Board                | LED type             | Pin                  |
| -------------------- | -------------------- | -------------------- |
| ESP32-C3-DevKitC-1   | Addressable          | GPIO8                |
| ESP32-C3-DevKitM-1   | Addressable          | GPIO8                |
| ESP32-S2-DevKitM-1   | Addressable          | GPIO18               |
| ESP32-S2-Saola-1     | Addressable          | GPIO18               |
| ESP32-S3-DevKitC-1   | Addressable          | GPIO48               |

See [Development Boards](https://www.espressif.com/en/products/devkits) for more information about it.

### Configure the Project

Open the project configuration menu (`idf.py menuconfig`). 

In the `Example Configuration` menu:

* Select the LED type in the `Blink LED type` option.
    * Use `GPIO` for regular LED blink.
    * Use `RMT` for addressable LED blink.
        * Use `RMT Channel` to select the RMT peripheral channel.
* Set the GPIO number used for the signal in the `Blink GPIO number` option.
* Set the blinking period in the `Blink period in ms` option.

### Build and Flash

Run `idf.py -p PORT flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Example Output

As you run the example, you will see the LED blinking, according to the previously defined period. For the addressable LED, you can also change the LED color by setting the `pStrip_a->set_pixel(pStrip_a, 0, 16, 16, 16);` (LED Strip, Pixel Number, Red, Green, Blue) with values from 0 to 255 in the `blink.c` file.

```
I (315) example: Example configured to blink addressable LED!
I (325) example: Turning the LED OFF!
I (1325) example: Turning the LED ON!
I (2325) example: Turning the LED OFF!
I (3325) example: Turning the LED ON!
I (4325) example: Turning the LED OFF!
I (5325) example: Turning the LED ON!
I (6325) example: Turning the LED OFF!
I (7325) example: Turning the LED ON!
I (8325) example: Turning the LED OFF!
```

Note: The color order could be different according to the LED model.

The pixel number indicates the pixel position in the LED strip. For a single LED, use 0.

## Troubleshooting

* If the LED isn't blinking, check the GPIO or the LED type selection in the `Example Configuration` menu.

For any technical queries, please open an [issue](https://github.com/espressif/esp-idf/issues) on GitHub. We will get back to you soon.
