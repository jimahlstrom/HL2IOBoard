// This is firmware for the Hermes Lite 2 IO board designed by Jim Ahlstrom, N2ADR. It is
//   Copyright (c) 2022-2023 James C. Ahlstrom <jahlstr@gmail.com>.
//   It is licensed under the MIT license. See MIT.txt.

#include <hardware/gpio.h>
#include <hardware/i2c.h>
#include <pico/i2c_slave.h>
#include <hardware/pwm.h>
#include <pico/binary_info.h>
#include <pico/stdlib.h>
#include <stdio.h>

#define I2C1_ADDRESS	0x1D
#define I2C1_BAUDRATE	(400 * 1000)
#define FT817_SLICE	4
#define FT817_CHAN	PWM_CHAN_A
#define FT817_WRAP	1000
#define FAN_SLICE	2
#define FAN_CHAN	PWM_CHAN_A
#define FAN_WRAP	1000

#define GPIO00_HPF	0
#define GPIO01_Sw12	1
#define GPIO02_RF3	2
#define GPIO03_INTTR	3
#define GPIO04_Fan	4
#define GPIO05_xxx	5
#define GPIO06_In5	6
#define GPIO07_In4	7
#define GPIO08_Out8	8
#define GPIO09_Out7	9
#define GPIO10_Out5	10
#define GPIO11_Out4	11
#define GPIO12_Sw5	12
#define GPIO13_EXTTR	13
#define GPIO14_I2C1_SDA	14
#define GPIO15_I2C1_SCL	15
#define GPIO16_Out1	16
#define GPIO17_In1	17
#define GPIO18_In2	18
#define GPIO19_Out2	19
#define GPIO20_Out3	20
#define GPIO21_In3	21
#define GPIO22_Out6	22
#define GPIO25_LED	25
#define GPIO26_ADC0	26
#define GPIO27_ADC1	27
#define GPIO28_ADC2	28

void configure_pins(int use_uart1, int use_pwm4a);
void test_pattern(void);
void configure_led_flasher(void);
void fast_led_flasher(void);
void i2c_slave_handler(i2c_inst_t *i2c, i2c_slave_event_t event);
void IrqRxTxChange(uint gpio, uint32_t events);
void ft817_band_volts(uint8_t band);
int tx_freq_to_band(uint64_t freq);

extern uint8_t firmware_version_major;
extern uint8_t firmware_version_minor;
extern uint64_t new_tx_freq;
