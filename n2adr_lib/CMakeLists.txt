cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
project(HL2IOBoard_n2adr_lib C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_library(hl2ioboard STATIC
	configure_pins.c
	led_flasher.c
	i2c_slave_handler.c
	ft817_band_volts.c
	tx_freq_to_band.c)
target_link_libraries(hl2ioboard
	pico_stdlib
	hardware_i2c
	hardware_pwm
	pico_i2c_slave)
