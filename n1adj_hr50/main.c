// This is firmware for the Hermes Lite 2 IO board designed by Jim Ahlstrom, N2ADR. It is
//   Copyright (c) 2022-2023 James C. Ahlstrom <jahlstr@gmail.com>.
//   Copyright (c) 2023 James Ancona <jim@anconafamily.com>.
//   It is licensed under the MIT license. See MIT.txt.

// This firmware outputs the Hardrock-50 band change commands on the serial port.

#include "../hl2ioboard.h"
#include "../i2c_registers.h"
#include "hardware/uart.h"

#define UART_ID uart0
#define BAUD_RATE 19200
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY    UART_PARITY_NONE

#define UART_TX_PIN 4

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

void hr50_change_band(uint8_t hr50_band) {
	char cmd[30];
	sprintf(cmd, "HRBN%d;", hr50_band);
	uart_puts(UART_ID, cmd);
	// printf("%s\n", cmd);
}

// void hr50_change_freq(uint64_t freq) {
// 	char cmd[30];
// 	sprintf(cmd, "FA%011d;", freq);
// 	uart_puts(UART_ID, cmd);
// 	printf("%s\n", cmd);
// }

int main()
{
	static uint8_t current_tx_fcode = 0;
	static bool current_is_rx = true;
	static uint8_t hr50_band = 99;
	uint8_t band, fcode;
	uint8_t i;
	uint64_t freq;

	stdio_init_all();
	configure_pins(true, false);
	configure_led_flasher();

   	uart_init(UART_ID, BAUD_RATE);
  	uart_set_hw_flow(UART_ID, false, false);
  	uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);
  	uart_set_fifo_enabled(UART_ID, true);


	while (1) {	// Wait for something to happen
		sleep_ms(1);	// This sets the polling frequency.
		// Poll for a changed Tx frequency. The new_tx_fcode is set in the I2C handler.
		if (current_tx_fcode != new_tx_fcode) {
			current_tx_fcode = new_tx_fcode;

			// We couldn't use this more straightforward approach because the frequency is an approximation. 
			// If we send it to the HR-50, often the amp says it's out-of-band even though the radio 
			// software is tuned to a valid frequency.
			//
			// freq = fcode2hertz(current_tx_fcode);
			// hr50_change_freq(freq);
			
			// So instead we convert the frequency code to an HR50 band code, taking
			// into account the frequency ranges associated with each code.
			hr50_band = fcode2hr50_band(current_tx_fcode);
			hr50_change_band(hr50_band);
		}
	}
}