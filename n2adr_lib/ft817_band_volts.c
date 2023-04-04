// This is firmware for the Hermes Lite 2 IO board designed by Jim Ahlstrom, N2ADR. It is
//   Copyright (c) 2022-2023 James C. Ahlstrom <jahlstr@gmail.com>.
//   It is licensed under the MIT license. See MIT.txt.

// Use PWM to output a voltage 0 to 5 volts on connector J4 pin 8 according to the band.
// You must set use_pwm4a in config_pins() to use this function.

#include "../hl2ioboard.h"

void ft817_band_volts(uint8_t band_index)	// Maximum voltage is 5000 mV
// Band    160     80    40      30      20      17      15      12      10       6       2    70cm
// Index     3      4     6       7       8       9      10      11      12      13      15      17
// Volts  0.33   0.67  1.00    1.33    1.67    2.00    2.33    2.67    3.00    3.33    3.67    4.00
{
	switch (band_index) {
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
