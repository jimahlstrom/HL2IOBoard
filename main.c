// This is the firmware for the Hermes Lite 2 IO board designed by Jim Ahlstrom, N2ADR. It is
//   Copyright (c) 2022 James C. Ahlstrom <jahlstr@gmail.com>.
//   It is licensed under the MIT license. See MIT.txt.

// The Hermes Lite 2 can read and write the I2C bus with C0 equal to 0x7D, C1 0x06 for write 0x07 for read, C2 the I2C address,
//   C3 the register number, and C4 the data.

// An I2C read from address 0x41 will return the hardware version in bits 3 to 0. This is currently 1, and
//    is independent of any software running on the board. The bits 7 to 4 are all one. So the return is 0xF1.

// The firmware I2C address is 0x1D. A read from this address and register zero returns the bus address plus four data bytes.
//    The data bytes are the firmware major version, the firmware minor version, the state of the input pins, and 0xFE.
//    The input pin bits are In5, In4, In3, In2, In1, Exttr.
//
// A write to address 0x1D and a register number controls the IO.  All writes are one byte. Available registers are:
//    Registers 0 to 3 are temporary buffers.
//    Register 11: The usage of the receive input J9 and the Pure Signal input J10.
//       0: The HL2 operates as usual. The receive input is not used. The Pure Signal input is available.
//       1: The receive input is used instead of the usual HL2 Rx input. Pure Signal is not available.
//       2: The receive input is used for Rx, and the Pure Signal input is used during Tx.
//    Register 12 is the fan voltage as a number from 0 to 255.
//    Register 13 is the least significant byte of the Tx frequency. Send the Tx frequency as MSB register 0, 1, 2, 3, 13 LSB.

#define USE_OUT8_FT817_BAND_VOLTS	1	// Output band volts on J4 pin 8.
#define USE_IN1_OUT1_FOR_UART		0	// UART Rx is J8 pin 1, UART Tx in J4 pin 1.

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "i2c_slave.h"
#include "i2c_fifo.h"

// These are the major and minor version numbers for standard firmware. If you make firmware for internal use and not for publication,
// please use a major version number starting with 200. If you publish firmware for a specific external amp, please use a
// major version other than 1.
#define FIRMWARE_VERSION_MAJOR		1
#define FIRMWARE_VERSION_MINOR		0

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

static uint8_t i2c_traffic = 0;
static uint8_t rx_input_usage = 0;
  // rx_input_usage == 0: Normal HL2 Rx input, J9 not used, Pure Signal at J10 available
  // rx_input_usage == 1: Use J9 for Rx input, Pure Signal at J10 is not available
  // rx_input_usage == 2: Use J9 for Rx input on Rx, use Pure Signal at J10 for Tx

static uint64_t new_tx_freq, current_tx_freq;

static void i2c_slave_handler(i2c_inst_t *i2c, i2c_slave_event_t event);
static bool led_flasher(repeating_timer_t * rt);
static void FT817BandVolts(uint8_t band);
static void TestPattern(void);
static void IrqRxTxChange(uint gpio, uint32_t events);
static void OnNewTxFreq(uint64_t freq);

int main()
{
	static repeating_timer_t led_timer;

	bi_decl(bi_program_description("This is the firmware for the N2ADR HermesLite2 IO Board."));
	bi_decl(bi_1pin_with_name(GPIO25_LED, "On-board LED"));

	stdio_init_all();

	// configure I2C1 for slave mode
	gpio_init(GPIO14_I2C1_SDA);	gpio_set_function(GPIO14_I2C1_SDA, GPIO_FUNC_I2C);
	gpio_init(GPIO15_I2C1_SCL);	gpio_set_function(GPIO15_I2C1_SCL, GPIO_FUNC_I2C);
	i2c_init(i2c1, I2C1_BAUDRATE);
	i2c_slave_init(i2c1, I2C1_ADDRESS, &i2c_slave_handler);

	// configure input pins
#if USE_IN1_OUT1_FOR_UART
	gpio_init(GPIO17_In1);		gpio_set_function(GPIO17_In1,  GPIO_FUNC_UART);	// UART0 RX
#else
	gpio_init(GPIO17_In1);		gpio_set_dir(GPIO17_In1, GPIO_IN);	gpio_disable_pulls(GPIO17_In1);
#endif
	gpio_init(GPIO18_In2);		gpio_set_dir(GPIO18_In2, GPIO_IN);	gpio_disable_pulls(GPIO18_In2);
	gpio_init(GPIO21_In3);		gpio_set_dir(GPIO21_In3, GPIO_IN);	gpio_disable_pulls(GPIO21_In3);
	gpio_init(GPIO07_In4);		gpio_set_dir(GPIO07_In4, GPIO_IN);	gpio_disable_pulls(GPIO07_In4);
	gpio_init(GPIO06_In5);		gpio_set_dir(GPIO06_In5, GPIO_IN);	gpio_disable_pulls(GPIO06_In5);
	gpio_init(GPIO13_EXTTR);	gpio_set_dir(GPIO13_EXTTR, GPIO_IN);	gpio_disable_pulls(GPIO13_EXTTR);

	// configure output pins
	gpio_init(GPIO25_LED);		gpio_set_dir(GPIO25_LED, GPIO_OUT);	gpio_put(GPIO25_LED, 0);
	gpio_init(GPIO00_HPF);		gpio_set_dir(GPIO00_HPF, GPIO_OUT);	gpio_put(GPIO00_HPF, 0);
	gpio_init(GPIO01_Sw12);		gpio_set_dir(GPIO01_Sw12, GPIO_OUT);	gpio_put(GPIO01_Sw12, 0);
	gpio_init(GPIO02_RF3);		gpio_set_dir(GPIO02_RF3, GPIO_OUT);	gpio_put(GPIO02_RF3, 0);
	gpio_init(GPIO03_INTTR);	gpio_set_dir(GPIO03_INTTR, GPIO_OUT);	gpio_put(GPIO03_INTTR, 0);
	gpio_init(GPIO04_Fan);		gpio_set_function(GPIO04_Fan,  GPIO_FUNC_PWM); 	// PWM2 A
	gpio_init(GPIO12_Sw5);		gpio_set_dir(GPIO12_Sw5, GPIO_OUT);	gpio_put(GPIO12_Sw5, 0);
#if USE_IN1_OUT1_FOR_UART
	gpio_init(GPIO16_Out1);		gpio_set_function(GPIO16_Out1, GPIO_FUNC_UART);	// UART0 TX
#else
	gpio_init(GPIO16_Out1);		gpio_set_dir(GPIO16_Out1, GPIO_OUT);	gpio_put(GPIO16_Out1, 0);
#endif
	gpio_init(GPIO19_Out2);		gpio_set_dir(GPIO19_Out2, GPIO_OUT);	gpio_put(GPIO19_Out2, 0);
	gpio_init(GPIO20_Out3);		gpio_set_dir(GPIO20_Out3, GPIO_OUT);	gpio_put(GPIO20_Out3, 0);
	gpio_init(GPIO11_Out4);		gpio_set_dir(GPIO11_Out4, GPIO_OUT);	gpio_put(GPIO11_Out4, 0);
	gpio_init(GPIO10_Out5);		gpio_set_dir(GPIO10_Out5, GPIO_OUT);	gpio_put(GPIO10_Out5, 0);
	gpio_init(GPIO22_Out6);		gpio_set_dir(GPIO22_Out6, GPIO_OUT);	gpio_put(GPIO22_Out6, 0);
	gpio_init(GPIO09_Out7);		gpio_set_dir(GPIO09_Out7, GPIO_OUT);	gpio_put(GPIO09_Out7, 0);
#if USE_OUT8_FT817_BAND_VOLTS
	gpio_init(GPIO08_Out8);		gpio_set_function(GPIO08_Out8,  GPIO_FUNC_PWM);	// PWM4 A
	pwm_set_wrap(FT817_SLICE, FT817_WRAP);
	pwm_set_chan_level(FT817_SLICE, FT817_CHAN, 0);
	pwm_set_enabled(FT817_SLICE, true);
#else
	gpio_init(GPIO08_Out8);		gpio_set_dir(GPIO08_Out8, GPIO_OUT);	gpio_put(GPIO08_Out8, 0);
#endif

	// unused pins
	//gpio_init(GPIO05_xxx);
	//gpio_init(GPIO26_ADC0);
	//gpio_init(GPIO27_ADC1);
	//gpio_init(GPIO28_ADC2);

	// Flash the LED fast for I2C traffic, slow otherwise
	add_repeating_timer_ms (50, led_flasher, NULL, &led_timer);

	while (1) {	// wait for something to happen
		sleep_ms(1);
		if (current_tx_freq != new_tx_freq) {
			current_tx_freq = new_tx_freq;
			OnNewTxFreq(current_tx_freq);
		}
		//TestPattern();
	}
}

static void i2c_slave_handler(i2c_inst_t *i2c, i2c_slave_event_t event)
{  // Receive and send I2C traffic. This is an IRQ so return quiskly!
	static uint8_t i2c_regs_control = 0xFF;		// the control (register) byte for receive or request
	static uint8_t i2c_control_valid = false;	// is i2c_regs_control valid?
	static uint8_t i2c_request_index = 0;		// the byte requested; 0, 1, 2, 3.
	static bool fan_enabled = false;
	static bool Exttr_enabled = false;
	uint8_t data;
	uint32_t level;
	static uint8_t buffer0, buffer1, buffer2, buffer3;	// temporary storage for multi-byte data

	switch (event) {
	case I2C_SLAVE_RECEIVE: // master has written data and this slave receives it
		data = i2c_read_byte(i2c);
		if ( ! i2c_control_valid) {	// the first byte is the control (register number)
			i2c_regs_control = data;
			i2c_control_valid = true;
			i2c_traffic = 10;	// flash the LED fast
		}
		else {
			switch (i2c_regs_control) {
			case 0:	
				buffer0 = data;
				break;
			case 1:
				buffer1 = data;
				break;
			case 2:
				buffer2 = data;
				break;
			case 3:
				buffer3 = data;
				break;
			case 11:		// How to use the external Rx input at J9
				rx_input_usage = data;
				switch (rx_input_usage) {
				case 0:
					gpio_put(GPIO03_INTTR, 0);
					gpio_put(GPIO02_RF3, 0);
					break;
				case 1:
					gpio_put(GPIO03_INTTR, 1);
					gpio_put(GPIO02_RF3, 1);
					break;
				case 2:
					gpio_put(GPIO03_INTTR, 1);
					IrqRxTxChange(GPIO13_EXTTR, 0xc);
					if ( ! Exttr_enabled) {
						Exttr_enabled = true;
						gpio_set_irq_enabled_with_callback (GPIO13_EXTTR, 0xc, 1, IrqRxTxChange);	// call on pin change
					}
					break;
				}
				break;
			case 12:		// fan control
				level = FAN_WRAP * data / 255;
				if (fan_enabled) {
					pwm_set_chan_level(FAN_SLICE, FAN_CHAN, level);
				}
				else {
					fan_enabled = true;
					pwm_set_wrap(FAN_SLICE, FAN_WRAP);
					pwm_set_chan_level(FAN_SLICE, FAN_CHAN, level);
					pwm_set_enabled(FAN_SLICE, true);
				}
				break;
			case 13:		// Tx frequency, LSB
				new_tx_freq = (uint64_t)buffer0 << 32;
				new_tx_freq |= data | buffer3 << 8 | buffer2 << 16 | buffer1 << 24;
			}
		}
		break;
	case I2C_SLAVE_REQUEST: // master is requesting data
		switch (i2c_regs_control) {
		case 0:		// Control address 0: Version Major, Version Minor, Input bits, 0xFE
				// Input bits: In5, In4, In3, In2, In1, EXTTR
			switch(i2c_request_index) {
			case 0:			// Index 0
				data = FIRMWARE_VERSION_MAJOR;
				break;
			case 1:			// Index 1
				data = FIRMWARE_VERSION_MINOR;
				break;
			case 2:			// Index 2: return the state of the input pins
				data = 0;
				if (gpio_get(GPIO06_In5))
					data |= 1 << 5;
				if (gpio_get(GPIO07_In4))
					data |= 1 << 4;
				if (gpio_get(GPIO21_In3))
					data |= 1 << 3;
				if (gpio_get(GPIO18_In2))
					data |= 1 << 2;
				if (gpio_get(GPIO17_In1))
					data |= 1 << 1;
				if (gpio_get(GPIO13_EXTTR))	// 1 for receive, 0 for transmit
					data |= 1;
				break;
			case 3:			// Index 3
				data = 0xFE;
				break;
			default:
				data = 0xFF;
				break;
			}
			break;
		default:
			data = 0xFF;
			break;
		}
		i2c_write_byte(i2c, data);
		i2c_request_index++;
		break;
	case I2C_SLAVE_FINISH: // master has signalled Stop or Restart
		i2c_control_valid = false;
		i2c_request_index = 0;
		break;
	}
}

static bool led_flasher(repeating_timer_t * rt)
{
	static uint8_t counter = 0;

	if (i2c_traffic) {
		counter = 99;
		i2c_traffic--;
	}
	if (counter++ >= 16) {
		counter = 0;
		if (gpio_get(GPIO25_LED))
			gpio_put(GPIO25_LED, 0);
		else
			gpio_put(GPIO25_LED, 1);
	}
	return true;
}

#define MAX_FREQ_INDEX	23
static void OnNewTxFreq(uint64_t freq)
{
// Band  137k  500k  160  80  60  40  30  20  17  15  12  10   6   4   2  1.25  70cm  33cm  23cm  13cm  9cm  5cm  3cm
// Index    1     2    3   4   5   6   7   8   9  10  11  12  13  14  15    16    17    18    19    20   21   22   23
static uint8_t index = 8;
// Table of cutoff frequencies. The correct index for the band is at: cutoff[index - 1] <= freq <= cutoff[index].
static uint64_t cutoff[MAX_FREQ_INDEX + 1] = {
          0,
     300000,	// 137k below this frequency
    1000000,	// 500k below this frequency
    2500000,	// 160m below this frequency
    4500000,	// 80m
    6000000,	// 60m
    8000000,	// 40m
   11000000,	// 30m
   16000000,	// 20m
   19000000,	// 17m
   22000000,	// 15m
   25000000,	// 12m
   32000000,	// 10m
   60000000,	// 6m
   90000000,	// 4m
  155000000,	// 2m
  300000000,	// 1.25
  500000000,	// 70cm
 1000000000,	// 33cm
 2000000000,	// 23cm
 3000000000,	// 13cm
 4000000000,	// 9cm
 7000000000,	// 5cm
11000000000,	// 3cm
} ;

	if (cutoff[index - 1] <= freq && freq <= cutoff[index]) {
		//printf ("Keep index %d\n", index);
		return;		// no change in band
	}
	for (index = 1; index < MAX_FREQ_INDEX; index++) {
		if (cutoff[index - 1] <= freq && freq <= cutoff[index])
			break;
	}
	//printf ("New index %d\n", index);
	if (index <= 3)
		gpio_put(GPIO00_HPF, 0);
	else
		gpio_put(GPIO00_HPF, 1);
#if USE_OUT8_FT817_BAND_VOLTS
	FT817BandVolts(index);
#endif
}

static void FT817BandVolts(uint8_t band)	// Maximum voltage is 5000 mV
// Band    160     80    40      30      20      17      15      12      10       6       2    70cm
// Index     3      4     6       7       8       9      10      11      12      13      15      17
// Volts  0.33   0.67  1.00    1.33    1.67    2.00    2.33    2.67    3.00    3.33    3.67    4.00
{
	switch (band) {
	case 3:
		pwm_set_chan_level(FT817_SLICE, FT817_CHAN, FT817_WRAP * 330 / 5000);
		break;
	case 4:
		pwm_set_chan_level(FT817_SLICE, FT817_CHAN, FT817_WRAP * 670 / 5000);
		break;
	case 5:
		pwm_set_chan_level(FT817_SLICE, FT817_CHAN, FT817_WRAP * 830 / 5000);
		break;
	case 6:
		pwm_set_chan_level(FT817_SLICE, FT817_CHAN, FT817_WRAP * 1000 / 5000);
		break;
	case 7:
		pwm_set_chan_level(FT817_SLICE, FT817_CHAN, FT817_WRAP * 1330 / 5000);
		break;
	case 8:
		pwm_set_chan_level(FT817_SLICE, FT817_CHAN, FT817_WRAP * 1670 / 5000);
		break;
	case 9:
		pwm_set_chan_level(FT817_SLICE, FT817_CHAN, FT817_WRAP * 2000 / 5000);
		break;
	case 10:
		pwm_set_chan_level(FT817_SLICE, FT817_CHAN, FT817_WRAP * 2330 / 5000);
		break;
	case 11:
		pwm_set_chan_level(FT817_SLICE, FT817_CHAN, FT817_WRAP * 2670 / 5000);
		break;
	case 12:
		pwm_set_chan_level(FT817_SLICE, FT817_CHAN, FT817_WRAP * 3000 / 5000);
		break;
	case 13:
		pwm_set_chan_level(FT817_SLICE, FT817_CHAN, FT817_WRAP * 3330 / 5000);
		break;
	case 15:
		pwm_set_chan_level(FT817_SLICE, FT817_CHAN, FT817_WRAP * 3670 / 5000);
		break;
	case 17:
		pwm_set_chan_level(FT817_SLICE, FT817_CHAN, FT817_WRAP * 4000 / 5000);
		break;
	default:
		pwm_set_chan_level(FT817_SLICE, FT817_CHAN, 0);
		break;
	}
}

static void IrqRxTxChange(uint gpio, uint32_t events)
{  // Called when EXTTR changes
	if (rx_input_usage == 2) {
		if (gpio_get(GPIO13_EXTTR))	// Rx
			gpio_put(GPIO02_RF3, 1);
		else		// Tx
			gpio_put(GPIO02_RF3, 0);
	}
}

static void TestPattern()
{  // Used to test IO board features.  Not used in production.
	static uint8_t tester = 0;
	static uint16_t fan = 0;

	tester++;
	fan++;
	// Toggle Switched 5 and 12 volt outputs
	gpio_put(GPIO01_Sw12, tester & 0x08);
	gpio_put(GPIO12_Sw5,  tester & 0x08);
	// count up on Out1 through Out7 (not Out8)
	gpio_put(GPIO16_Out1, tester & 0x01);
	gpio_put(GPIO19_Out2, tester & 0x02);
	gpio_put(GPIO20_Out3, tester & 0x04);
	gpio_put(GPIO11_Out4, tester & 0x08);
	gpio_put(GPIO10_Out5, tester & 0x10);
	gpio_put(GPIO22_Out6, tester & 0x20);
	gpio_put(GPIO09_Out7, tester & 0x40);
#if ! USE_OUT8_FT817_BAND_VOLTS
	gpio_put(GPIO08_Out8, tester & 0x80);
#endif
}
