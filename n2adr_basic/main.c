// This is firmware for the Hermes Lite 2 IO board designed by Jim Ahlstrom, N2ADR. It is
//   Copyright (c) 2022-2023 James C. Ahlstrom <jahlstr@gmail.com>.
//   It is licensed under the MIT license. See MIT.txt.

// This firmware outputs the band index on connector J4 and the FT817 band voltage on J4 pin 8.

#include "../hl2ioboard.h"
#include "../i2c_registers.h"

// These are the major and minor version numbers for firmware.
uint8_t firmware_version_major=1;
uint8_t firmware_version_minor=2;

static uint8_t current_tx_fcode;

int main()
{
	uint8_t band;

	stdio_init_all();
	configure_pins(false, true);
	configure_led_flasher();

	while (1) {	// Wait for something to happen
		sleep_ms(1);	// This sets the polling frequency.
		// Control the Icom AH-4 antenna tuner.
		// Assume the START line is on J4 pin 6 and the KEY line is on J8 pin 2.
		// The control register is
		IcomAh4(GPIO22_Out6, GPIO18_In2);
		// Poll for a changed Tx frequency. The new_tx_fcode is set in the I2C handler.
		if (current_tx_fcode != new_tx_fcode) {
			current_tx_fcode = new_tx_fcode;
			band = fcode2band(current_tx_fcode);		// Convert the frequency code to a band code.
			ft817_band_volts(band);				// Put the band voltage on J4 pin 8.
			switch (band) {		// Set some GPIO pins according to the band
			case BAND_40:
			case BAND_15:
				gpio_put(GPIO16_Out1, 1);
				gpio_put(GPIO19_Out2, 0);
				gpio_put(GPIO20_Out3, 0);
				break;
			case BAND_20:
				gpio_put(GPIO16_Out1, 0);
				gpio_put(GPIO19_Out2, 1);
				gpio_put(GPIO20_Out3, 0);
				break;
			case BAND_10:
				gpio_put(GPIO16_Out1, 0);
				gpio_put(GPIO19_Out2, 0);
				gpio_put(GPIO20_Out3, 1);
				break;
			default:
				gpio_put(GPIO16_Out1, 0);
				gpio_put(GPIO19_Out2, 0);
				gpio_put(GPIO20_Out3, 0);
			}
		}
	}
}
