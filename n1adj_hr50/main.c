// This is firmware for the Hermes Lite 2 IO board designed by Jim Ahlstrom, N2ADR. It is
//   Copyright (c) 2022-2023 James C. Ahlstrom <jahlstr@gmail.com>.
//   Copyright (c) 2023 James Ancona <jim@anconafamily.com>.
//   It is licensed under the MIT license. See MIT.txt.

// This firmware uses the Hardrock-50 serial port commands to do band changes
// and trigger the built-in antenna tuner.

#include "../hl2ioboard.h"
#include "../i2c_registers.h"
#include "hardware/uart.h"
#include <string.h>
#include <math.h>

#define UART_ID uart0
#define BAUD_RATE 19200
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY UART_PARITY_NONE

#define DEBUG false

// These are the major and minor version numbers for firmware. You must set these.
uint8_t firmware_version_major=1;
uint8_t firmware_version_minor=1;

// Translate a frequency code to an HL50 band code. Out-of-band codes are mapped to
// 99 (Unknown), not the nearest band. That way the HR 50 will refuse to transmit
// when far out-of-band.
uint8_t fcode2hr50_band(uint8_t frequency_code) {
	uint8_t hr50_band = 99;

	//  Band         F1           F2    Code1  Code2      Decode1      Decode2
	//   160     1.800000     2.000000     71     72     1.845712     1.968962
	if (frequency_code >= 71 && frequency_code <= 72) {
		hr50_band = 10;
	//    80     3.500000     4.000000     81     83     3.522876     4.009073
	} else if (frequency_code >= 81 && frequency_code <= 83) {
		hr50_band = 9;
	//    60     5.300000     5.430000     87     88     5.192032     5.538736
	} else if (frequency_code >= 87 && frequency_code <= 88) {
		hr50_band = 8;
	//    40     7.000000     7.300000     92     92     7.173053     7.173053
	} else if (frequency_code == 92) {
		hr50_band = 7;
	//    30    10.100000    10.150000     97     97     9.909932     9.909932
	} else if (frequency_code == 97) {
		hr50_band = 6;
	//    20    14.000000    14.350000    102    103    13.691069    14.605307
	} else if (frequency_code >= 102 && frequency_code <= 103) {
		hr50_band = 5;
	//    17    18.068000    18.168000    106    106    17.730897    17.730897
	} else if (frequency_code == 106) {
		hr50_band = 4;
	//    15    21.000000    21.450000    109    109    21.525374    21.525374
	} else if (frequency_code == 109) {
		hr50_band = 3;
	//    12    24.890000    24.990000    111    111    24.496124    24.496124
	} else if (frequency_code == 111) {
		hr50_band = 2;
	//    10    28.000000    29.700000    113    114    27.876872    29.738385
	} else if (frequency_code >= 113 && frequency_code <= 114) {
		hr50_band = 1;
	//     6    50.000000    54.000000    122    123    49.877428    53.208055
	} else if (frequency_code >= 122 && frequency_code <= 123) {
		hr50_band = 0;
	}
	return hr50_band;
}

uint8_t response[256] = "";
bool response_ok = false;
bool response_ready = false;

void clear_response() {
	response[0] = '\0';
	response_ready = false;
	response_ok = false;
}

void uart_puts_log(uart_inst_t *uart, const char *s) {
	clear_response();
	uart_puts(uart, s);
#if DEBUG
	printf("sent: %s\n", s);
#endif
}

// RX interrupt handler
void on_uart_rx() {
	while (uart_is_readable(UART_ID)) {
		if (response_ready == true) {
#if DEBUG
			printf("discarding: %s\n", response);
#endif
			// Throw away the old response and start again
			clear_response();
		}
		uint8_t ch = uart_getc(UART_ID);
		size_t len = strlen(response);
		if (len < 255) {
			strncat(response, &ch, 1);
		} else {
			// buffer is full
			response_ok = false;
			response_ready = true;
		}
		if (ch == '\n') {
			// response should end ";\r\n"
			if (len > 2 && response[len-2] == ';' && response[len-1] == '\r') {
				// remove the "\r\n"
				response[len-1] = '\0';
				response_ok = true;
			} else {
				response_ok = false;
			}
			response_ready = true;
		}
	}
}

void hr50_change_band(uint8_t hr50_band) {
	char cmd[30];
	sprintf(cmd, "HRBN%d;", hr50_band);
	uart_puts_log(UART_ID, cmd);
}

// void hr50_change_freq(uint64_t hr50_freq) {
// 	char cmd[30];
// 	sprintf(cmd, "FA%011d;", hr50_freq);
// 	uart_puts_log(UART_ID, cmd);
// }

#define RESPONSE_BAD 0
#define RESPONSE_GOOD 1
#define RESPONSE_NOT_READY 3

uint8_t handle_serial_response(char *expected_response, char *next_command[2]) {
	uint8_t status = RESPONSE_BAD;
	if (response_ready == true) {
#if DEBUG
		printf("response: %s\n", response);
#endif
		if (response_ok) {
			if (strstr(response, expected_response) != NULL) {
				status = RESPONSE_GOOD;
				for (int i = 0; i < 2; i++) {
					if (strlen(next_command[i]) > 0) {
						uart_puts_log(UART_ID, next_command[i]);
					}
				}
			}
		}
		clear_response();
	} else {
		status = RESPONSE_NOT_READY;
	}
	return status;
}

// Calculate SWR as a rolling average of the last k samples
// Return zero if we determine that transmit voltage readings aren't available
double read_swr() {
	const double conversion_factor = 3.3 / (1 << 12);
	const double k = 10.0;
	double v_rev, v_fwd, ratio, swr;
	static double avg_swr = 5.0;

	adc_select_input(0);
	v_rev = adc_read() * conversion_factor;
	adc_select_input(1);
	v_fwd = adc_read() * conversion_factor;
	if (v_rev > v_fwd) {
		// This should never happen, and probably means that the transmit voltage readings
		// aren't available because they haven't been wired up.
		avg_swr = 0.0;
	} else {
		if (v_fwd < 0.0001) v_fwd = 0.0001;
		ratio = v_rev / v_fwd;
		if (ratio > 0.9999) ratio = 0.9999;
		swr = (1 + ratio)/(1 - ratio);
		avg_swr = ((avg_swr * (k - 1.0)) + swr) / k;
	}
	return avg_swr;
}

uint8_t console_in[256] = "";

void hr50_tune() {
	static uint8_t state_antenna_tuner = 0;
#if DEBUG
	static uint8_t tuner_reg_value = 255, old_state_antenna_tuner = 255;
#endif
	static absolute_time_t tuner_time0, tuner_time1;
	uint8_t reg;

	static double swr, last_swr = 100.0;
	static absolute_time_t last_swr_change_time, last_log_time;

#if DEBUG
	if (response_ready == true) {
		if (response_ok) {
			printf("good: %s\n", response);
		} else {
			printf("bad: %s\n", response);
		}
	}
#endif

	// // Allow command input from USB
	// int ch = getchar_timeout_us(100);
	// while (ch != PICO_ERROR_TIMEOUT) {
	// 	printf("%c", ch);			   // Echo back so you can see what you've done

	// 	if (ch == '\r') {
	// 		uart_puts_log(UART_ID, console_in);
	// 		console_in[0] = '\0';
	// 	} else {
	// 		uint8_t c = ch;
	// 		strncat(console_in, &c, 1);
	// 	}
	// 	ch = getchar_timeout_us(100);
	// }

#if DEBUG
	if (tuner_reg_value != Registers[REG_ANTENNA_TUNER] || state_antenna_tuner != old_state_antenna_tuner) {
		tuner_reg_value = Registers[REG_ANTENNA_TUNER];
		printf("tuner register: 0x%02x, state_antenna_tuner: %d\n", tuner_reg_value, state_antenna_tuner);
		old_state_antenna_tuner = state_antenna_tuner;
	}
#endif
	// Error if we've been tuning for more than 10 seconds
	if (state_antenna_tuner && absolute_time_diff_us(tuner_time0, get_absolute_time()) / 1000 >= 10000) {
		state_antenna_tuner = 0;
		Registers[REG_ANTENNA_TUNER] = 0xF0;
	}
	switch (state_antenna_tuner) {
	case 0:		// Check the I2C register. 1 is start tuning, 2 is bypass mode.
		reg = Registers[REG_ANTENNA_TUNER];
		if (reg == 1 || reg == 2) {
			state_antenna_tuner = reg;
			tuner_time0 = get_absolute_time();
		}
		break;
	case 1: 	// Start tuning
		// Set ATU active
		uart_puts_log(UART_ID, "HRAT2;");
		// Query ATU Mode
		uart_puts_log(UART_ID, "HRAT;");
		state_antenna_tuner = 4;
		Registers[REG_ANTENNA_TUNER] = state_antenna_tuner;
		break;
	case 2: 	// Bypass tuner
		// Set ATU to bypass
		uart_puts_log(UART_ID, "HRAT1;");
		// Query ATU Mode
		uart_puts_log(UART_ID, "HRAT;");
		state_antenna_tuner = 3;
		Registers[REG_ANTENNA_TUNER] = state_antenna_tuner;
		break;
	case 3: 	// Check ATU mode
		{
			char *cmd[2] = {"",""};
			uint8_t status = handle_serial_response("HRAT1;", cmd);
			if (status == RESPONSE_GOOD) {
				state_antenna_tuner = 0;
				Registers[REG_ANTENNA_TUNER] = state_antenna_tuner;
			} else if (status == RESPONSE_BAD) {
				state_antenna_tuner = 0;
				Registers[REG_ANTENNA_TUNER] = 0xF1;
			}
		}
		break;
	case 4: 	// Check ATU mode, then tune on next TX
		{
			char *cmd[2] = {"HRTU1;","HRTU;"};
			uint8_t status = handle_serial_response("HRAT2;", cmd);
			if (status == RESPONSE_GOOD) {
				state_antenna_tuner = 5;
				Registers[REG_ANTENNA_TUNER] = state_antenna_tuner;
			} else if (status == RESPONSE_BAD) {
				state_antenna_tuner = 0;
				Registers[REG_ANTENNA_TUNER] = 0xF2;
			}
		}
		break;
	case 5:		// Check tuner mode, then trigger radio TX
		{
			char *cmd[2] = {"",""};
			uint8_t status = handle_serial_response("HRTU1;", cmd);
			if (status == RESPONSE_GOOD) {
				state_antenna_tuner = 6;
				Registers[REG_ANTENNA_TUNER] = 0xEE;	// Radio should start TX
				tuner_time1 = get_absolute_time();
			} else if (status == RESPONSE_BAD) {
				state_antenna_tuner = 0;
				Registers[REG_ANTENNA_TUNER] = 0xF3;
			}
		}
		break;
	case 6: 	// Wait for tuning to start
		if (absolute_time_diff_us(tuner_time1, get_absolute_time()) / 1000 >= 500) {
			uart_puts_log(UART_ID, "HRTMS;");
			state_antenna_tuner = 7;
			tuner_time1 = get_absolute_time();
		}
		break;
	case 7: 	// Check if transmit started
		if (absolute_time_diff_us(tuner_time1, get_absolute_time()) / 1000 >= 500) {
			// The HR-50 serial port is disabled during TX, so if we get a reponse, transmit didn't start
			char *cmd[2] = {"",""};
			uint8_t status = handle_serial_response("HRTMS;", cmd);
#if DEBUG
			printf("status: %d\n", status);
#endif
			if (status != RESPONSE_NOT_READY) {
				// Transmit didn't start, so abort
				state_antenna_tuner = 0;
				// Turn off transmitter
				Registers[REG_ANTENNA_TUNER] = 0xF4;
			} else {
				state_antenna_tuner = 8;
				tuner_time1 = last_swr_change_time = last_log_time = get_absolute_time();
				last_swr = 100.0;
			}
		}
		break;
	case 8: 	// Wait for tuning to finish
		// We can't communicate with the amp while it's transmitting.
		// Instead, we can monitor SWR from the HL2 if it's available, or wait for a fixed amount of time.
		// In my testing, tuning seems to either succeed or fail in less than 5 seconds.
		swr = read_swr();
		if (swr < 1.0 || fabs((swr - last_swr)/last_swr) > 0.1) {
			// Either swr changed by > 10%, or we got a bad reading.
			// SWR less less than one indicates we can't trust it, so we update last_swr_change_time
			// which means it will never appear to stabilize and we'll revert to timed transmit.
			last_swr_change_time = get_absolute_time();
			last_swr = swr;
		}
#if DEBUG
		if ((absolute_time_diff_us(last_log_time, get_absolute_time()) / 1000 >= 100)) {
			printf("%5d swr: %7.2f last_swr: %7.2f last_swr_change_time: %5d delta: %5d\n",
				(uint32_t)(absolute_time_diff_us(tuner_time1, get_absolute_time()) / 1000),
				swr,
				last_swr,
				(uint32_t)(absolute_time_diff_us(tuner_time1, last_swr_change_time) / 1000),
				(uint32_t)(absolute_time_diff_us(last_swr_change_time, get_absolute_time()) / 1000));
			last_log_time = get_absolute_time();
		}
#endif
		if (absolute_time_diff_us(last_swr_change_time, get_absolute_time()) / 1000 >= 1000 ||
			absolute_time_diff_us(tuner_time1, get_absolute_time()) / 1000 >= 5000) {
			// Turn off transmitter if SWR has stabilized for one second or five seconds has elapsed
			state_antenna_tuner = 9;
			Registers[REG_ANTENNA_TUNER] = state_antenna_tuner;
			tuner_time1 = get_absolute_time();
		}
		break;
	case 9:		// Wait for TX to stop and serial to start responding again
		// It takes more than a second for the amp to start responding again.
		if (absolute_time_diff_us(tuner_time1, get_absolute_time()) / 1000 >= 2000) {
			// Query for tuning success
			uart_puts_log(UART_ID, "HRTMS;");
			state_antenna_tuner = 10;
		}
		Registers[REG_ANTENNA_TUNER] = state_antenna_tuner;
		break;
	case 10:
		{
			char *cmd[2] = {"",""};
			uint8_t status = handle_serial_response("HRTMS;", cmd);
			if (status == RESPONSE_GOOD) {
				// Tuning succeeded
				Registers[REG_ANTENNA_TUNER] = 0;
				state_antenna_tuner = 0;
			} else if (status == RESPONSE_BAD) {
				// Tuning failed
				Registers[REG_ANTENNA_TUNER] = 0xFF;
				state_antenna_tuner = 0;
			}
		}
		break;
	}
}

int main()
{
	uint8_t current_tx_fcode = 0;
	uint8_t hr50_band = 99;

	stdio_init_all();
	configure_pins(true, false);
	configure_led_flasher();

   	uart_init(UART_ID, BAUD_RATE);
  	uart_set_hw_flow(UART_ID, false, false);
  	uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);
  	uart_set_fifo_enabled(UART_ID, true);

	// Set up a RX interrupt
	// We need to set up the handler first
	// Select correct interrupt for the UART we are using
	int UART_IRQ = UART_ID == uart0 ? UART0_IRQ : UART1_IRQ;

	// And set up and enable the interrupt handlers
	irq_set_exclusive_handler(UART_IRQ, on_uart_rx);
	irq_set_enabled(UART_IRQ, true);

	// Now enable the UART to send interrupts - RX only
	uart_set_irq_enables(UART_ID, true, false);

	while (1) {	// Wait for something to happen
		sleep_ms(1);	// This sets the polling frequency.

		// Poll for a changed Tx frequency. The new_tx_freq is set in the I2C handler.
		// Frequency changes using this approach seemed to be unreliable--the amp often
		// ended up in a "UNK" state for some reason.
		// if (current_tx_freq != new_tx_freq) {
		// 	current_tx_freq = new_tx_freq;
		// 	hr50_change_freq(new_tx_freq);
		// }

		// Poll for a changed Tx frequency code. The new_tx_fcode is set in the I2C handler.
		if (current_tx_fcode != new_tx_fcode) {
			current_tx_fcode = new_tx_fcode;
			// We convert the frequency code to an HR50 band code, taking
			// into account the frequency ranges associated with each code.
			hr50_band = fcode2hr50_band(current_tx_fcode);
			hr50_change_band(hr50_band);
		}

		hr50_tune();
	}
}