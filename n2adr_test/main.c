// This is firmware for the Hermes Lite 2 IO board designed by Jim Ahlstrom, N2ADR. It is
//   Copyright (c) 2022-2023 James C. Ahlstrom <jahlstr@gmail.com>.
//   It is licensed under the MIT license. See MIT.txt.

// This is used to test IO board features.
// It outputs 3.0 volts on J4 pin 8, and sets the fan to 5.7 volts.
// It outputs pulses of 1,2,3,4,5,6,7 milliseconds on J4 pins 1 to 7.
// It toggles the 5 and 12 volt switched outputs at 500 and 250 Hz.
// It generates a test pattern for the antenna tuner protocol.

// This is also used by the test fixture. Write 1 to register 111 to stop the test pattern and clear GPIO.

#include "../hl2ioboard.h"
#include "../i2c_registers.h"

// These are the major and minor version numbers for firmware.
uint8_t firmware_version_major=2;
uint8_t firmware_version_minor=11;

static void clear_gpio(void);

int main()
{
	uint8_t tester = 0;
	uint32_t antenna_tuner = 0;

	stdio_init_all();
	configure_pins(false, true);
	pwm_set_chan_level(FAN_SLICE, FAN_CHAN, FAN_WRAP * 128 / 255);
	configure_led_flasher();
	ft817_band_volts(BAND_10);		// 3.0 volts

	while (1) {
		sleep_ms(1);
		if (Registers[111] == 1)
			clear_gpio();
		if (Registers[111] != 0)	// used by the IO board test fixture
			continue;
		// Generate test pattern:
		tester++;
		// Toggle Switched 5 and 12 volt outputs
		gpio_put(GPIO01_Sw12, tester & 0x02);
		gpio_put(GPIO12_Sw5,  tester & 0x01);
		switch (tester & 0x0F) {	// ouptput pulse of 1,2,3,4,5,6,7 milliseconds
		case 0:
			gpio_put(GPIO16_Out1, 1);
			gpio_put(GPIO19_Out2, 1);
			gpio_put(GPIO20_Out3, 1);
			gpio_put(GPIO11_Out4, 1);
			gpio_put(GPIO10_Out5, 1);
			gpio_put(GPIO22_Out6, 1);
			gpio_put(GPIO09_Out7, 1);
			break;
		case 1:
			gpio_put(GPIO16_Out1, 0);
			break;
		case 2:
			gpio_put(GPIO19_Out2, 0);
			break;
		case 3:
			gpio_put(GPIO20_Out3, 0);
			break;
		case 4:
			gpio_put(GPIO11_Out4, 0);
			break;
		case 5:
			gpio_put(GPIO10_Out5, 0);
			break;
		case 6:
			gpio_put(GPIO22_Out6, 0);
			break;
		case 7:
			gpio_put(GPIO09_Out7, 0);
			break;
		}
		if (Registers[REG_ANTENNA_TUNER] == 1) {
			Registers[REG_ANTENNA_TUNER] = 3;
			antenna_tuner = 1;
		}
		if (antenna_tuner == 0)
			continue;
		antenna_tuner++;
		if (antenna_tuner < 3000)
			Registers[REG_ANTENNA_TUNER] = 3;
		else if (antenna_tuner < 6000)
			Registers[REG_ANTENNA_TUNER] = 4;
		else if (antenna_tuner < 9000)
			Registers[REG_ANTENNA_TUNER] = 0xEE;
		else if (antenna_tuner < 12000)
			Registers[REG_ANTENNA_TUNER] = 16;
		else {
			Registers[REG_ANTENNA_TUNER] = 0;
			antenna_tuner = 0;
		}
			
	}
}

static void clear_gpio(void)
{
	Registers[111] = 2;
	gpio_put(GPIO01_Sw12, 0);
	gpio_put(GPIO12_Sw5, 0);
	gpio_put(GPIO16_Out1, 0);
	gpio_put(GPIO19_Out2, 0);
	gpio_put(GPIO20_Out3, 0);
	gpio_put(GPIO11_Out4, 0);
	gpio_put(GPIO10_Out5, 0);
	gpio_put(GPIO22_Out6, 0);
	gpio_put(GPIO09_Out7, 0);
}
