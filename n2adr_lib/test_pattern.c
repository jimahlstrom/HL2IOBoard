// This is firmware for the Hermes Lite 2 IO board designed by Jim Ahlstrom, N2ADR. It is
//   Copyright (c) 2022-2023 James C. Ahlstrom <jahlstr@gmail.com>.
//   It is licensed under the MIT license. See MIT.txt.

// This toggles the outputs on J4 pins 1 to 7 and toggles the 5 and 12 volt outputs.

#include "../hl2ioboard.h"

void test_pattern()
{  // Used to test IO board features.  Not used in production.
	static uint8_t tester = 0;

	tester++;
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
}

