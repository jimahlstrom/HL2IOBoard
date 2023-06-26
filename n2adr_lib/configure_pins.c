// This is firmware for the Hermes Lite 2 IO board designed by Jim Ahlstrom, N2ADR. It is
//   Copyright (c) 2022-2023 James C. Ahlstrom <jahlstr@gmail.com>.
//   It is licensed under the MIT license. See MIT.txt.

#include "../hl2ioboard.h"

// unused pins
//gpio_init(GPIO05_xxx);
//gpio_init(GPIO26_ADC0);
//gpio_init(GPIO27_ADC1);
//gpio_init(GPIO28_ADC2);

// Configure all pins on the Pico. If use_uart1, use the UART on J4 pin1 and J8 pin 1.
// If use_pwm4a, use pulse width modulation on J4 pin 8. This must be set in order to use ft817_band_volts().

void configure_pins(bool use_uart1, bool use_pwm4a)
{
	// configure I2C1 for slave mode
	gpio_init(GPIO14_I2C1_SDA);	gpio_set_function(GPIO14_I2C1_SDA, GPIO_FUNC_I2C);
	gpio_init(GPIO15_I2C1_SCL);	gpio_set_function(GPIO15_I2C1_SCL, GPIO_FUNC_I2C);
	i2c_init(i2c1, I2C1_BAUDRATE);
	i2c_slave_init(i2c1, I2C1_ADDRESS, &i2c_slave_handler);

	// configure input pins
	if (use_uart1) {
		gpio_init(GPIO17_In1);
		gpio_set_function(GPIO17_In1,  GPIO_FUNC_UART);	// UART0 RX
	}
	else {
		gpio_init(GPIO17_In1);
		gpio_set_dir(GPIO17_In1, GPIO_IN);
		gpio_disable_pulls(GPIO17_In1);
	}
	gpio_init(GPIO18_In2);		gpio_set_dir(GPIO18_In2, GPIO_IN);	gpio_disable_pulls(GPIO18_In2);
	gpio_init(GPIO21_In3);		gpio_set_dir(GPIO21_In3, GPIO_IN);	gpio_disable_pulls(GPIO21_In3);
	gpio_init(GPIO07_In4);		gpio_set_dir(GPIO07_In4, GPIO_IN);	gpio_disable_pulls(GPIO07_In4);
	gpio_init(GPIO06_In5);		gpio_set_dir(GPIO06_In5, GPIO_IN);	gpio_disable_pulls(GPIO06_In5);
	gpio_init(GPIO13_EXTTR);	gpio_set_dir(GPIO13_EXTTR, GPIO_IN);	gpio_disable_pulls(GPIO13_EXTTR);
	gpio_set_irq_enabled_with_callback (GPIO13_EXTTR, 0xc, 1, IrqRxTxChange);	// call on pin change

	// configure output pins
	gpio_init(GPIO25_LED);		gpio_set_dir(GPIO25_LED, GPIO_OUT);	gpio_put(GPIO25_LED, 0);
	gpio_init(GPIO00_HPF);		gpio_set_dir(GPIO00_HPF, GPIO_OUT);	gpio_put(GPIO00_HPF, 0);
	gpio_init(GPIO01_Sw12);		gpio_set_dir(GPIO01_Sw12, GPIO_OUT);	gpio_put(GPIO01_Sw12, 0);
	gpio_init(GPIO02_RF3);		gpio_set_dir(GPIO02_RF3, GPIO_OUT);	gpio_put(GPIO02_RF3, 0);
	gpio_init(GPIO03_INTTR);	gpio_set_dir(GPIO03_INTTR, GPIO_OUT);	gpio_put(GPIO03_INTTR, 0);
	gpio_init(GPIO04_Fan);		gpio_set_function(GPIO04_Fan,  GPIO_FUNC_PWM); 	// PWM2 A
	pwm_set_wrap(FAN_SLICE, FAN_WRAP);	// configure fan
	pwm_set_chan_level(FAN_SLICE, FAN_CHAN, 0);
	pwm_set_enabled(FAN_SLICE, true);
	gpio_init(GPIO12_Sw5);		gpio_set_dir(GPIO12_Sw5, GPIO_OUT);	gpio_put(GPIO12_Sw5, 0);
	if (use_uart1) {
		gpio_init(GPIO16_Out1);
		gpio_set_function(GPIO16_Out1, GPIO_FUNC_UART);	// UART0 TX
	}
	else {
		gpio_init(GPIO16_Out1);
		gpio_set_dir(GPIO16_Out1, GPIO_OUT);
		gpio_put(GPIO16_Out1, 0);
	}
	gpio_init(GPIO19_Out2);		gpio_set_dir(GPIO19_Out2, GPIO_OUT);	gpio_put(GPIO19_Out2, 0);
	gpio_init(GPIO20_Out3);		gpio_set_dir(GPIO20_Out3, GPIO_OUT);	gpio_put(GPIO20_Out3, 0);
	gpio_init(GPIO11_Out4);		gpio_set_dir(GPIO11_Out4, GPIO_OUT);	gpio_put(GPIO11_Out4, 0);
	gpio_init(GPIO10_Out5);		gpio_set_dir(GPIO10_Out5, GPIO_OUT);	gpio_put(GPIO10_Out5, 0);
	gpio_init(GPIO22_Out6);		gpio_set_dir(GPIO22_Out6, GPIO_OUT);	gpio_put(GPIO22_Out6, 0);
	gpio_init(GPIO09_Out7);		gpio_set_dir(GPIO09_Out7, GPIO_OUT);	gpio_put(GPIO09_Out7, 0);
	if (use_pwm4a) {	// PWM4 A
		gpio_init(GPIO08_Out8);
		gpio_set_function(GPIO08_Out8,  GPIO_FUNC_PWM);
		pwm_set_wrap(FT817_SLICE, FT817_WRAP);
		pwm_set_chan_level(FT817_SLICE, FT817_CHAN, 0);
		pwm_set_enabled(FT817_SLICE, true);
	}
	else {
		gpio_init(GPIO08_Out8);
		gpio_set_dir(GPIO08_Out8, GPIO_OUT);
		gpio_put(GPIO08_Out8, 0);
	}
}
