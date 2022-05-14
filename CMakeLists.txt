cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
project(HL2IOBoard_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_subdirectory(i2c_slave)

add_executable(main main.c)
pico_enable_stdio_usb(main 1)
pico_add_extra_outputs(main)
target_link_libraries(main pico_stdlib hardware_i2c hardware_pwm i2c_slave)
target_include_directories(main PUBLIC "${PROJECT_SOURCE_DIR}/i2c_slave/include")
