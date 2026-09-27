cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
project(HL2IOBoard_dl4jc_juma C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

# C++ rather than C: the status parser and the band table are shared with an
# ESP32 firmware that is C++, and they are the parts worth not duplicating.
add_executable(main main.cpp juma_status.cpp bands.cpp)
target_include_directories(main PRIVATE ${CMAKE_CURRENT_LIST_DIR}/..)
target_compile_options(main PRIVATE -Wall -Wextra)

# All four are settings as well as build options: every bit of REG_JUMA_MODE can
# be written over I2C or over the USB console at any time, and the mode is kept
# in flash. These only decide what a board comes up with.
option(JUMA_HOLD_OPERATE "Hold the PA in OPERATE from power-up" ON)
option(JUMA_TELEMETRY    "A status line per second on USB from power-up" ON)
option(JUMA_PROXY        "The USB port is the PA's serial port from power-up" OFF)
option(JUMA_DEBUG        "Trace the PA traffic on the USB port" OFF)

set(JUMA_MODE_AT_BOOT 0)
if(JUMA_HOLD_OPERATE)
	math(EXPR JUMA_MODE_AT_BOOT "${JUMA_MODE_AT_BOOT} + 2")
endif()
if(JUMA_TELEMETRY)
	math(EXPR JUMA_MODE_AT_BOOT "${JUMA_MODE_AT_BOOT} + 4")
endif()
if(JUMA_PROXY)
	math(EXPR JUMA_MODE_AT_BOOT "${JUMA_MODE_AT_BOOT} + 8")
endif()
if(JUMA_DEBUG)
	target_compile_definitions(main PRIVATE JUMA_DEBUG=1)
endif()
target_compile_definitions(main PRIVATE JUMA_MODE_AT_BOOT=${JUMA_MODE_AT_BOOT})

# The USB write blocks while a host is attached but not reading, and the default
# is half a second - long enough to eat into the PA's 5 s remote timeout.
target_compile_definitions(main PRIVATE PICO_STDIO_USB_STDOUT_TIMEOUT_US=10000)

pico_enable_stdio_usb(main 1)
pico_enable_stdio_uart(main 0)
pico_add_extra_outputs(main)

target_link_libraries(main
	pico_stdlib
	hardware_i2c
	hardware_pwm
	hardware_uart
	hardware_adc
	pico_i2c_slave
	${PROJECT_SOURCE_DIR}/../n2adr_lib/build/libhl2ioboard.a)
