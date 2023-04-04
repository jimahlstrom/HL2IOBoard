// This is firmware for the Hermes Lite 2 IO board designed by Jim Ahlstrom, N2ADR. It is
//   Copyright (c) 2022-2023 James C. Ahlstrom <jahlstr@gmail.com>.
//   It is licensed under the MIT license. See MIT.txt.

// This firmware toggles pins 1 to 7 on connector J4, and toggles the 5 and 12 volt outputs.
// It outputs 3.00 volts on J4 pin 8, and sets the fan to 0.5 * VSUP.

#include "../hl2ioboard.h"

// These are the major and minor version numbers for firmware.
uint8_t firmware_version_major=1;
uint8_t firmware_version_minor=0;

int main()
{
	stdio_init_all();
	configure_pins(0, 1);
	pwm_set_chan_level(FAN_SLICE, FAN_CHAN, FAN_WRAP * 128 / 255);
	configure_led_flasher();
	ft817_band_volts(12);		// 3.00 volts

	while (1) {	// wait for something to happen
		sleep_ms(1);
		test_pattern();
	}
}
