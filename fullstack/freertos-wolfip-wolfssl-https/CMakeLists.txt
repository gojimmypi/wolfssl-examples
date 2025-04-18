cmake_minimum_required(VERSION 3.13)
project(freertos_wolfssl_demo C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# wolfSSL configuration
add_definitions(-DWOLFSSL_USER_SETTINGS)
add_definitions(-DWOLFSSL_WOLFIP)

# FreeRTOS Kernel source files for POSIX port
set(FREERTOS_PORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/freertos/FreeRTOS-Kernel/portable/ThirdParty/GCC/Posix)
set(FREERTOS_HEAP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/freertos/FreeRTOS-Kernel/portable/MemMang)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/freertos/FreeRTOS-Kernel/include
    ${FREERTOS_PORT_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../wolfip/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../wolfip
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../wolfip/src/http
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../wolfip/src/port
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../wolfssl
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../wolfssl/include
)

# FreeRTOS source files
set(FREERTOS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/freertos/FreeRTOS-Kernel/tasks.c
    ${CMAKE_CURRENT_SOURCE_DIR}/freertos/FreeRTOS-Kernel/queue.c
    ${CMAKE_CURRENT_SOURCE_DIR}/freertos/FreeRTOS-Kernel/list.c
    ${CMAKE_CURRENT_SOURCE_DIR}/freertos/FreeRTOS-Kernel/timers.c
    ${CMAKE_CURRENT_SOURCE_DIR}/freertos/FreeRTOS-Kernel/event_groups.c
    ${CMAKE_CURRENT_SOURCE_DIR}/freertos/FreeRTOS-Kernel/stream_buffer.c
    ${FREERTOS_PORT_DIR}/port.c
    ${FREERTOS_HEAP_DIR}/heap_3.c
    ${CMAKE_CURRENT_SOURCE_DIR}/freertos/utils/utils.c
)

# Add wolfIP library
add_library(wolfip STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../wolfip/src/wolfip.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../wolfip/src/http/httpd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../wolfip/src/port/wolfssl_io.c
)

# Add the main application
add_executable(freertos_sim
    ${FREERTOS_SOURCES}
    src/main.c
    src/wolfip_freertos.c
    src/https_server.c
)

target_link_libraries(freertos_sim
    pthread
    wolfip
    wolfssl
)
