// This is firmware for the Hermes Lite 2 IO board designed by Jim Ahlstrom, N2ADR. It is
//   Copyright (c) 2022-2023 James C. Ahlstrom <jahlstr@gmail.com>.
//   It is licensed under the MIT license. See MIT.txt.

// Use PWM to output a voltage 0 to 5 volts on connector J4 pin 8 according to the band.
// You must set use_pwm4a in config_pins() to use this function.

#include "../hl2ioboard.h"
#include "../i2c_registers.h"

void J4Pin8_millivolts(uint16_t millivolts)	// Maximum voltage is 5000 millivolts
{
	uint32_t level;

	level = FT817_WRAP * millivolts;
	level = (level + 2500) / 5000;
	if (level > FT817_WRAP)
		level = FT817_WRAP;
	pwm_set_chan_level(FT817_SLICE, FT817_CHAN, (uint16_t)level);
	Registers[GPIO_DIRECT_BASE + GPIO08_Out8] = level / 4;
}

void ft817_band_volts(uint8_t band_code)	// Maximum voltage is 5000 mV
{
	switch (band_code) {
	case BAND_160:
		J4Pin8_millivolts(330);
		break;
	case BAND_80:
		J4Pin8_millivolts(670);
		break;
	case BAND_60:
		J4Pin8_millivolts(830);
		break;
	case BAND_40:
		J4Pin8_millivolts(1000);
		break;
	case BAND_30:
		J4Pin8_millivolts(1330);
		break;
	case BAND_20:
		J4Pin8_millivolts(1670);
		break;
	case BAND_17:
		J4Pin8_millivolts(2000);
		break;
	case BAND_15:
		J4Pin8_millivolts(2330);
		break;
	case BAND_12:
		J4Pin8_millivolts(2670);
		break;
	case BAND_10:
		J4Pin8_millivolts(3000);
		break;
	case BAND_6:
		J4Pin8_millivolts(3330);
		break;
	case BAND_2:
		J4Pin8_millivolts(3670);
		break;
	case BAND_70cm:
		J4Pin8_millivolts(4000);
		break;
	default:
		J4Pin8_millivolts(0);
		break;
	}
}

void xiegu_band_volts(uint8_t band_code)	// Maximum voltage is 5000 mV
{
	switch (band_code) {
	case BAND_160:
		J4Pin8_millivolts(230);
		break;
	case BAND_80:
		J4Pin8_millivolts(460);
		break;
	case BAND_60:
		J4Pin8_millivolts(690);
		break;
	case BAND_40:
		J4Pin8_millivolts(920);
		break;
	case BAND_30:
		J4Pin8_millivolts(1150);
		break;
	case BAND_20:
		J4Pin8_millivolts(1380);
		break;
	case BAND_17:
		J4Pin8_millivolts(1610);
		break;
	case BAND_15:
		J4Pin8_millivolts(1840);
		break;
	case BAND_12:
		J4Pin8_millivolts(2070);
		break;
	case BAND_10:
		J4Pin8_millivolts(2300);
		break;
	default:
		J4Pin8_millivolts(0);
		break;
	}
}

