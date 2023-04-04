// This is firmware for the Hermes Lite 2 IO board designed by Jim Ahlstrom, N2ADR. It is
//   Copyright (c) 2022-2023 James C. Ahlstrom <jahlstr@gmail.com>.
//   It is licensed under the MIT license. See MIT.txt.

// This firmware outputs the band index on connector J4 and the FT817 band voltage on J4 pin 8.

#include "../hl2ioboard.h"

// These are the major and minor version numbers for firmware.
uint8_t firmware_version_major=1;
uint8_t firmware_version_minor=0;

static uint64_t current_tx_freq;

int main()
{
	int index;

	stdio_init_all();
	configure_pins(0, 1);
	configure_led_flasher();

	while (1) {	// wait for something to happen
		sleep_ms(1);
		if (current_tx_freq != new_tx_freq) {
			current_tx_freq = new_tx_freq;
			index = tx_freq_to_band(current_tx_freq);
			ft817_band_volts(index);
			gpio_put(GPIO16_Out1, index & 0x01);
			gpio_put(GPIO19_Out2, index & 0x02);
			gpio_put(GPIO20_Out3, index & 0x04);
			gpio_put(GPIO11_Out4, index & 0x08);
			gpio_put(GPIO10_Out5, index & 0x10);
		}
	}
}
