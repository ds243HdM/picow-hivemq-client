cmake_minimum_required(VERSION 3.12)

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)

project(picow_iot C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.3.0")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.3.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

# Initialize the SDK
pico_sdk_init()

add_compile_options(-Wall
        -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
        -Wno-unused-function # we have some for the docs that aren't called
        -Wno-maybe-uninitialized
        )

if (PICO_CYW43_SUPPORTED) # set by BOARD=pico-w
    if (NOT TARGET pico_cyw43_arch)
        message("Skipping build as support is not available")
    else()
        add_executable(picow_iot
            picow_iot.c
        )

        # enable usb output, disable uart output
        pico_enable_stdio_usb(picow_iot 1)
        pico_enable_stdio_uart(picow_iot 0)

        target_compile_definitions(picow_iot PRIVATE
            TEST_TCP_SERVER_IP=\"${TEST_TCP_SERVER_IP}\"
            NO_SYS=1
            # ALTCP and TLS Support (Fixes the undefined reference error)
            LWIP_ALTCP=1
            LWIP_ALTCP_TLS=1
            LWIP_ALTCP_TLS_MBEDTLS=1
        )

        target_include_directories(picow_iot PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}
            ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts
        )

        target_link_libraries(picow_iot
            pico_stdlib
            pico_cyw43_arch_lwip_poll
            pico_lwip_mqtt
            pico_lwip_mbedtls    
            pico_mbedtls
        )

        pico_add_extra_outputs(picow_iot)
    endif()
endif()

