// This is firmware for the Hermes Lite 2 IO board designed by Jim Ahlstrom, N2ADR. It is
//   Copyright (c) 2022-2023 James C. Ahlstrom <jahlstr@gmail.com>.
//   It is licensed under the MIT license. See MIT.txt.

// Use PWM to output a voltage 0 to 5 volts on connector J4 pin 8 according to the band.
// You must set use_pwm4a in config_pins() to use this function.

#include "../hl2ioboard.h"

void ft817_band_volts(uint8_t band_code)	// Maximum voltage is 5000 mV
// Band    160     80    40      30      20      17      15      12      10       6       2    70cm
// Volts  0.33   0.67  1.00    1.33    1.67    2.00    2.33    2.67    3.00    3.33    3.67    4.00
{
	switch (band_code) {
	case BAND_160:
		pwm_set_chan_level(FT817_SLICE, FT817_CHAN, FT817_WRAP * 330 / 5000);
		break;
	case BAND_80:
		pwm_set_chan_level(FT817_SLICE, FT817_CHAN, FT817_WRAP * 670 / 5000);
		break;
	case BAND_60:
		pwm_set_chan_level(FT817_SLICE, FT817_CHAN, FT817_WRAP * 830 / 5000);
		break;
	case BAND_40:
		pwm_set_chan_level(FT817_SLICE, FT817_CHAN, FT817_WRAP * 1000 / 5000);
		break;
	case BAND_30:
		pwm_set_chan_level(FT817_SLICE, FT817_CHAN, FT817_WRAP * 1330 / 5000);
		break;
	case BAND_20:
		pwm_set_chan_level(FT817_SLICE, FT817_CHAN, FT817_WRAP * 1670 / 5000);
		break;
	case BAND_17:
		pwm_set_chan_level(FT817_SLICE, FT817_CHAN, FT817_WRAP * 2000 / 5000);
		break;
	case BAND_15:
		pwm_set_chan_level(FT817_SLICE, FT817_CHAN, FT817_WRAP * 2330 / 5000);
		break;
	case BAND_12:
		pwm_set_chan_level(FT817_SLICE, FT817_CHAN, FT817_WRAP * 2670 / 5000);
		break;
	case BAND_10:
		pwm_set_chan_level(FT817_SLICE, FT817_CHAN, FT817_WRAP * 3000 / 5000);
		break;
	case BAND_6:
		pwm_set_chan_level(FT817_SLICE, FT817_CHAN, FT817_WRAP * 3330 / 5000);
		break;
	case BAND_2:
		pwm_set_chan_level(FT817_SLICE, FT817_CHAN, FT817_WRAP * 3670 / 5000);
		break;
	case BAND_70cm:
		pwm_set_chan_level(FT817_SLICE, FT817_CHAN, FT817_WRAP * 4000 / 5000);
		break;
	default:
		pwm_set_chan_level(FT817_SLICE, FT817_CHAN, 0);
		break;
	}
}
