// This is firmware for the Hermes Lite 2 IO board designed by Jim Ahlstrom, N2ADR. It is
//   Copyright (c) 2022-2023 James C. Ahlstrom <jahlstr@gmail.com>.
//   It is licensed under the MIT license. See MIT.txt.

// This firmware outputs the band index on connector J4 and the FT817 band voltage on J4 pin 8.

#include "../hl2ioboard.h"
#include "../i2c_registers.h"

// These are the major and minor version numbers for firmware.
uint8_t firmware_version_major=1;
uint8_t firmware_version_minor=2;

static uint64_t current_tx_freq;
static uint8_t current_tx_band;

int main()
{
	stdio_init_all();
	configure_pins(false, true);
	configure_led_flasher();

	while (1) {	// Wait for something to happen
		sleep_ms(1);	// This sets the polling frequency.
		// Control the Icom AH-4 antenna tuner.
		// Assume the START line is on J4 pin 6 and the KEY line is on J8 pin 2.
		// The control register is
		IcomAh4(GPIO22_Out6, GPIO18_In2);
		// Poll for a changed Tx frequency. The new_tx_freq is set in the I2C handler.
		if (current_tx_freq != new_tx_freq) {
			current_tx_freq = new_tx_freq;
			current_tx_band = tx_freq_to_band(current_tx_freq);	// Convert the frequency to a band index.
			ft817_band_volts(current_tx_band);			// Put the band voltage on J4 pin 8.
			gpio_put(GPIO16_Out1, current_tx_band & 0x01);		// Put the binary band index on J4 pins 1 to 5.
			gpio_put(GPIO19_Out2, current_tx_band & 0x02);
			gpio_put(GPIO20_Out3, current_tx_band & 0x04);
			gpio_put(GPIO11_Out4, current_tx_band & 0x08);
			gpio_put(GPIO10_Out5, current_tx_band & 0x10);
		}
	}
}
