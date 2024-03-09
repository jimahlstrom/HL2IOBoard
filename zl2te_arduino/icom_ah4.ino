// This is firmware for the Hermes Lite 2 IO board designed by Jim Ahlstrom, N2ADR. It is
//   Copyright (c) 2022-2023 James C. Ahlstrom <jahlstr@gmail.com>.
//   It is licensed under the MIT license. See MIT.txt.

// This implements an Icon AH-4 antenna tuner. The radio monitors progress by reading the register REG_ANTENNA_TUNER.
// Write 1 for a tune request, 2 to select bypass mode. Other tuners may have additional low numbered requests.
// As tuning progresses, reading the register will show advancing numbers indicating progress.
// These register values are fixed:
//    Zero indicates a successful completion.
//    0xEE is a command to send RF to the tuner. Stop RF when the 0xEE ends.
//    Numbers 0xF0 and higher indicate a failure.

//#include "hl2ioboard.h"
//#include "i2c_registers.h"

void IcomAh4(uint8_t AH4_START, uint8_t AH4_KEY)
{
	uint8_t reg;
	static uint8_t state_antenna_tuner = 0;
	static absolute_time_t tuner_time0;

	// Start a timed loop for an Icom AH-4 antenna tuner.
	// See https://hamoperator.com/HF/AH-4_Design_and_Operation.pdf.
	if (state_antenna_tuner && absolute_time_diff_us(tuner_time0, get_absolute_time ()) / 1000 >= 4000) {
		gpio_put(AH4_START, 0);
		state_antenna_tuner = 0;
		Registers[REG_ANTENNA_TUNER] = 0xFD;
	}
	switch (state_antenna_tuner) {
	case 0:		// Check the I2C register. 1 is start tuning, 2 is bypass mode.
		reg = Registers[REG_ANTENNA_TUNER];
		if (reg == 1 || reg == 2) {
			state_antenna_tuner = reg;
			tuner_time0 = get_absolute_time ();
		}
		break;
	case 1:		// Starting state for tuning.
		if (gpio_get(AH4_KEY) == 0) {	// There is no connection to KEY
			state_antenna_tuner = 0;
			Registers[REG_ANTENNA_TUNER] = 0xFA;
		}
		else {
			gpio_put(AH4_START, 1);
			state_antenna_tuner = 4;
			Registers[REG_ANTENNA_TUNER] = state_antenna_tuner;
		}
		break;
	case 2:		// Starting state for bypass mode.
		gpio_put(AH4_START, 1);
		state_antenna_tuner = 3;
		Registers[REG_ANTENNA_TUNER] = state_antenna_tuner;
		break;
	case 3:		// Bypass mode: Timer for 70 milliseconds
		if (absolute_time_diff_us(tuner_time0, get_absolute_time ()) / 1000 >= 70) {
			gpio_put(AH4_START, 0);
			state_antenna_tuner = 0;
			Registers[REG_ANTENNA_TUNER] = 0;	// Success
		}
		break;
	case 4:		// Tuning: wait up to 600 milliseconds for KEY to go low
		if (gpio_get(AH4_KEY) == 0) {
			state_antenna_tuner = 5;
			Registers[REG_ANTENNA_TUNER] = 0xEE;	// Radio should start CW Tx with 5 to 15 watts power
			tuner_time0 = get_absolute_time ();	// Reset timer
		}
		else if (absolute_time_diff_us(tuner_time0, get_absolute_time ()) / 1000 >= 600) {
			gpio_put(AH4_START, 0);
			state_antenna_tuner = 0;
			Registers[REG_ANTENNA_TUNER] = 0xFB;	// timeout with no KEY received
		}
		break;
	case 5:		// Tuning:  remove START 250 milliseconds after KEY
		if (absolute_time_diff_us(tuner_time0, get_absolute_time ()) / 1000 >= 250) {
			gpio_put(AH4_START, 0);
			state_antenna_tuner = 6;
		}
		break;
	case 6:		// Tuning: Wait for KEY to go high
		if (gpio_get(AH4_KEY)) {	// KEY went high
			state_antenna_tuner = 7;
			Registers[REG_ANTENNA_TUNER] = state_antenna_tuner;	// Radio should stop CW Tx
			tuner_time0 = get_absolute_time ();	// Reset timer
		}
		break;
	case 7:		// Tuning: Wait to see if the AH-4 pulses KEY low to indicate failure
		if (absolute_time_diff_us(tuner_time0, get_absolute_time ()) / 1000 >= 60) {
			state_antenna_tuner = 0;
			if (gpio_get(AH4_KEY) == 0)
				Registers[REG_ANTENNA_TUNER] = 0xFC;	// Failure
			else
				Registers[REG_ANTENNA_TUNER] = 0;	// Success
		}
		break;
	}
}