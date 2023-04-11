// This is firmware for the Hermes Lite 2 IO board designed by Jim Ahlstrom, N2ADR. It is
//   Copyright (c) 2022-2023 James C. Ahlstrom <jahlstr@gmail.com>.
//   It is licensed under the MIT license. See MIT.txt.

// This is an interrupt service routine for I2C traffic. It must return quickly.

#include "../hl2ioboard.h"

static uint8_t rx_input_usage = 0;
  // rx_input_usage == 0: Normal HL2 Rx input, J9 not used, Pure Signal at J10 available
  // rx_input_usage == 1: Use J9 for Rx input, Pure Signal at J10 is not available
  // rx_input_usage == 2: Use J9 for Rx input on Rx, use Pure Signal at J10 for Tx

uint64_t new_tx_freq;

void i2c_slave_handler(i2c_inst_t *i2c, i2c_slave_event_t event)
{  // Receive and send I2C traffic. This is an IRQ so return quiskly!
	static uint8_t i2c_regs_control = 0xFF;		// the control (register) byte for receive or request
	static uint8_t i2c_control_valid = false;	// is i2c_regs_control valid?
	static uint8_t i2c_request_index = 0;		// the byte requested; 0, 1, 2, 3.
	uint8_t data;
	static uint8_t buffer0, buffer1, buffer2, buffer3;	// temporary storage for multi-byte data

	switch (event) {
	case I2C_SLAVE_RECEIVE: // master has written data and this slave receives it
		data = i2c_read_byte_raw(i2c);
		if ( ! i2c_control_valid) {	// the first byte is the control (register number)
			i2c_regs_control = data;
			i2c_control_valid = true;
			fast_led_flasher();
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
					break;
				}
				break;
			case 12:		// fan control
				pwm_set_chan_level(FAN_SLICE, FAN_CHAN, FAN_WRAP * data / 255);
				break;
			case 13:		// Tx frequency, LSB
				new_tx_freq = (uint64_t)data | (uint64_t)buffer3 << 8 | (uint64_t)buffer2 << 16
					| (uint64_t)buffer1 << 24 | (uint64_t)buffer0 << 32;	// Thanks to Neil, G4BRK
				if (new_tx_freq <= 2500000)
					gpio_put(GPIO00_HPF, 0);
				else
					gpio_put(GPIO00_HPF, 1);
				break;
			}
		}
		break;
	case I2C_SLAVE_REQUEST: // master is requesting data
		switch (i2c_regs_control) {
		case 0:		// Control address 0: Version Major, Version Minor, Input bits, 0xFE
				// Input bits: In5, In4, In3, In2, In1, EXTTR
			switch(i2c_request_index) {
			case 0:			// Index 0
				data = firmware_version_major;
				break;
			case 1:			// Index 1
				data = firmware_version_minor;
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
		i2c_write_byte_raw(i2c, data);
		i2c_request_index++;
		break;
	case I2C_SLAVE_FINISH: // master has signalled Stop or Restart
		i2c_control_valid = false;
		i2c_request_index = 0;
		break;
	}
}

void IrqRxTxChange(uint gpio, uint32_t events)
{  // Called when EXTTR changes
	if (rx_input_usage == 2) {
		if (gpio_get(GPIO13_EXTTR))	// Rx
			gpio_put(GPIO02_RF3, 1);
		else		// Tx
			gpio_put(GPIO02_RF3, 0);
	}
}
