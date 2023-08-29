// This is firmware for the Hermes Lite 2 IO board designed by Jim Ahlstrom, N2ADR. It is
//   Copyright (c) 2022-2023 James C. Ahlstrom <jahlstr@gmail.com>.
//   It is licensed under the MIT license. See MIT.txt.

#include <hardware/gpio.h>
#include <hardware/i2c.h>
#include <pico/i2c_slave.h>
#include <hardware/pwm.h>
#include <hardware/adc.h>
#include <pico/binary_info.h>
#include <pico/stdlib.h>
#include <stdio.h>

#define I2C1_ADDRESS	0x1D
#define I2C1_BAUDRATE	(400 * 1000)
#define FT817_SLICE	4
#define FT817_CHAN	PWM_CHAN_A
#define FT817_WRAP	1020
#define FAN_SLICE	2
#define FAN_CHAN	PWM_CHAN_A
#define FAN_WRAP	1020

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

#define BAND_137k	31	// Frequency 0.139 MHz
#define BAND_500k	50	// Frequency 0.475 MHz
#define BAND_160	71	// Frequency 1.846 MHz
#define BAND_80		82	// Frequency 3.758 MHz
#define BAND_60		88	// Frequency 5.539 MHz
#define BAND_40		92	// Frequency 7.173 MHz
#define BAND_30		97	// Frequency 9.910 MHz
#define BAND_20		103	// Frequency 14.605 MHz
#define BAND_17		106	// Frequency 17.731 MHz
#define BAND_15		109	// Frequency 21.525 MHz
#define BAND_12		111	// Frequency 24.496 MHz
#define BAND_10		114	// Frequency 29.738 MHz
#define BAND_6		123	// Frequency 53.208 MHz
#define BAND_4		127	// Frequency 68.908 MHz
#define BAND_2		139	// Frequency 149.676 MHz
#define BAND_125cm	145	// Frequency 220.592 MHz
#define BAND_70cm	155	// Frequency 421.041 MHz
#define BAND_33cm	167	// Frequency 914.543 MHz
#define BAND_23cm	172	// Frequency 1263.487 MHz
#define BAND_13cm	182	// Frequency 2411.594 MHz
#define BAND_9cm	187	// Frequency 3331.738 MHz
#define BAND_5cm	196	// Frequency 5961.160 MHz
#define BAND_3cm	204	// Frequency 9998.100 MHz

typedef void (*irq_handler)(uint8_t register_number, uint8_t register_datum);

void configure_pins(bool use_uart1, bool use_pwm4a);
void configure_led_flasher(void);
void fast_led_flasher(void);
void i2c_slave_handler(i2c_inst_t *i2c, i2c_slave_event_t event);
void IrqRxTxChange(uint gpio, uint32_t events);
void J4Pin8_millivolts(uint16_t millivolts);
void ft817_band_volts(uint8_t band);
void xiegu_band_volts(uint8_t band);
uint8_t tx_freq_to_band(uint64_t freq);
void IcomAh4(uint8_t, uint8_t);
uint8_t hertz2fcode(uint64_t hertz);
uint64_t fcode2hertz(uint8_t fcode);
uint8_t fcode2band(uint8_t fcode);

extern uint8_t firmware_version_major;
extern uint8_t firmware_version_minor;
extern uint64_t new_tx_freq;
extern uint8_t new_tx_fcode;
extern bool rx_freq_changed;
extern uint8_t Registers[256];
extern irq_handler IrqHandler[256];
