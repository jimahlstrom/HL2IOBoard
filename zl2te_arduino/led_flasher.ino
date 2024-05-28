// This is firmware for the Hermes Lite 2 IO board designed by Jim Ahlstrom, N2ADR. It is
//   Copyright (c) 2022-2023 James C. Ahlstrom <jahlstr@gmail.com>.
//   It is licensed under the MIT license. See MIT.txt.

// This controls the Pico on-board LED. It flashed slowly until I2C traffic is received.

//#include "hl2ioboard.h"

// Flash the LED fast for I2C traffic, slow otherwise
static uint8_t fast_flasher = 0;

static bool led_flasher(repeating_timer_t * rt)
{
	static uint8_t counter = 0;

	if (fast_flasher) {
		counter = 99;
		fast_flasher--;
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

void configure_led_flasher(void)
{
	static repeating_timer_t led_timer;

	add_repeating_timer_ms (50, led_flasher, NULL, &led_timer);
}

void fast_led_flasher(void)
{
	fast_flasher = 10;
}