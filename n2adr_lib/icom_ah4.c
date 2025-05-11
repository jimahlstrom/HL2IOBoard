// This is firmware for the Hermes Lite 2 IO board designed by Jim Ahlstrom, N2ADR. It is
//   Copyright (c) 2022-2025 James C. Ahlstrom <jahlstr@gmail.com>.
//   It is licensed under the MIT license. See MIT.txt.
// Additional documentation added in May, 2025 by Graeme, ZL2TE.
// N2ADR fixed a bug in the tuning error pulse detection May, 2025.

// This implements an Icom AH-4 antenna tuner. The radio starts a tune by writing 1 or 2 to REG_ANTENNA_TUNER.
// The radio monitors progress by reading the register REG_ANTENNA_TUNER.
// Write 1 for a tune request, 2 to select bypass mode. Other tuners may have additional low numbered requests.
// As tuning progresses, reading the register will show advancing numbers indicating progress.
// These register values are fixed:
//    Zero indicates a successful completion.
//    0xEE is a command from the AH-4 to send RF to the tuner. Stop RF when the 0xEE ends.
//    Numbers 0xF0 and higher indicate a failure.

// For AH-4 documentation see:	https://hamoperator.com/HF/AH-4_Design_and_Operation.pdf.

#include "../hl2ioboard.h"
#include "../i2c_registers.h"

void IcomAh4(uint8_t AH4_START, uint8_t AH4_KEY)
{
	uint8_t reg;
	static uint8_t state_antenna_tuner = 0;
	static absolute_time_t tuner_time0;

	// Timeout safety:
	// Ensure the tuner does not remain in an active state indefinitely.
	// If the tuner has been in any active state for 4000 ms (4 seconds), abort.
	if (state_antenna_tuner && absolute_time_diff_us(tuner_time0, get_absolute_time ()) / 1000 >= 4000) {
		gpio_put(AH4_START, 0);			// Deassert START signal to abort
		state_antenna_tuner = 0;		// Reset state machine
		Registers[REG_ANTENNA_TUNER] = 0xFD;	// Indicate timeout failure to host
	}
	switch (state_antenna_tuner) {
	case 0:		// Check the I2C register. 1 is start tuning, 2 is bypass mode.
			// This state acts as the resting/default state for the state machine.
			// It waits for the host to write a control value to REG_ANTENNA_TUNER.
		reg = Registers[REG_ANTENNA_TUNER];
		if (reg == 1 || reg == 2) {			// The user wrote 1 or 2. Otherwise do nothing.
			state_antenna_tuner = reg;		// Go to state 1 or 2.
			tuner_time0 = get_absolute_time ();	// Start timer.
		}
		break;
	case 1:		// The user wrote 1. Starting state for tuning.
		if (gpio_get(AH4_KEY) == 0) {			// There is no connection to KEY
			state_antenna_tuner = 0;		// Abort and return to idle
			Registers[REG_ANTENNA_TUNER] = 0xFA;	// Indicate failure (KEY not high)
		}
		else {
			gpio_put(AH4_START, 1);					// Assert START to begin tuning
			state_antenna_tuner = 4;				// Go to KEY-low waiting state
			Registers[REG_ANTENNA_TUNER] = state_antenna_tuner;	// Notify host
		}
		break;
	case 2:		// The user wrote 2. Starting state for bypass mode.
		gpio_put(AH4_START, 1);					// Begin bypass pulse
		state_antenna_tuner = 3;				// Go to pulse delay state
		Registers[REG_ANTENNA_TUNER] = state_antenna_tuner;	// Inform host
		break;
	case 3:		// Bypass mode: Timer for 70 milliseconds
		if (absolute_time_diff_us(tuner_time0, get_absolute_time ()) / 1000 >= 70) {
			gpio_put(AH4_START, 0);			// Complete the bypass pulse
			state_antenna_tuner = 0;		// Return to idle
			Registers[REG_ANTENNA_TUNER] = 0;	// Report success to host
		}
		break;
	case 4:		// Tuning: wait up to 600 milliseconds for KEY to go low.
			// The AH-4 pulls KEY low when ready for RF input.
			// If it doesn't happen within 600 ms, assume failure.
		if (gpio_get(AH4_KEY) == 0) {
			state_antenna_tuner = 5;		// Proceed to RF delay state
			Registers[REG_ANTENNA_TUNER] = 0xEE;	// Instruct host to begin CW Tx with 5 to 15 watts power
			tuner_time0 = get_absolute_time ();	// Reset timer
		}
		else if (absolute_time_diff_us(tuner_time0, get_absolute_time ()) / 1000 >= 600) {
			gpio_put(AH4_START, 0);			// Abort
			state_antenna_tuner = 0;
			Registers[REG_ANTENNA_TUNER] = 0xFB;	// Report timeout with no KEY received
		}
		break;
	case 5:		// Tuning:  remove START 250 milliseconds after KEY
		if (absolute_time_diff_us(tuner_time0, get_absolute_time ()) / 1000 >= 250) {
			gpio_put(AH4_START, 0);		// Now safe to release START
			state_antenna_tuner = 6;	// Proceed to final KEY-high wait
		}
		break;
	case 6:		// Tuning: Wait for KEY to go high
		if (gpio_get(AH4_KEY)) {	// The AH-4 pulls KEY high when tuning is complete.
			state_antenna_tuner = 7;				// Ready for error-check delay
			Registers[REG_ANTENNA_TUNER] = state_antenna_tuner;	// Radio should stop CW Tx
			tuner_time0 = get_absolute_time ();			// Start 60 ms error window
		}
		break;
	case 7:		// Tuning: Wait to see if the AH-4 pulses the KEY low to indicate failure.
			// The KEY starts high. If the AH-4 was unable to achieve tuning, it pulses the KEY low for 20 ms
			// and then changes it to high. Waiting 60 ms after KEY goes high after tuning
			// provides a confirmation read during the possible error pulse window.
		if (absolute_time_diff_us(tuner_time0, get_absolute_time ()) / 1000 >= 60) {
			state_antenna_tuner = 0;		// Return to idle
			Registers[REG_ANTENNA_TUNER] = 0;	// Success
		}
		if (gpio_get(AH4_KEY) == 0) {			// Detected error pulse
			state_antenna_tuner = 0;		// Return to idle
			Registers[REG_ANTENNA_TUNER] = 0xFC;	// Report error pulse to host
		}
		break;
	}
}
