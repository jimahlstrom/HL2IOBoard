// This is firmware for the Hermes Lite 2 IO board designed by Jim Ahlstrom, N2ADR. It is
//   Copyright (c) 2022-2023 James C. Ahlstrom <jahlstr@gmail.com>.
//   Copyright (c) 2023 James Ancona <jim@anconafamily.com>.
//   Copyright (c) 2024 Glitsun Cheeran <glitsun@gmail.com>.
//   It is licensed under the MIT license. See MIT.txt.

// This firmware uses the SPE Expert serial port commands to do band changes

#include "../hl2ioboard.h"
#include "../i2c_registers.h"
#include "hardware/uart.h"
#include <string.h>
#include <stdlib.h>
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


void spe_change_frequency(uint64_t spe_freq) {
	char cmd[40];
	sprintf(cmd, "IF%011d0000+0000000000010000000;", spe_freq);
	uart_puts_log(UART_ID, cmd);
}

void spe_update_rf_power(uint64_t spe_rfpower) {
	char cmd[40];
	sprintf(cmd, "PC%03d;", spe_rfpower);
	uart_puts_log(UART_ID, cmd);
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
		
		if (len > 1 && response[len] == ';') {
			response_ok = true;
			break;
		} else {
			response_ok = false;
		}
	}

	
	size_t len = strlen(response);
	
#if DEBUG
	printf("Message: %s(%d)\n", response,len);
#endif
	if(response_ok){
		if(strcmp((char *) response, "IF;") == 0){
			if(new_tx_freq == 0){
				//Default to 40m 7.150MHz
				spe_change_frequency(7150000);
			} else {
				spe_change_frequency(new_tx_freq);
			}
		}
		if(strstr(response,"PC") && strstr(response,";")){
			char cmd[40];
			if(len>3){
				char rfpower[3];
				//Copying and replying the same incoming rfpower from SPE
				strncpy(rfpower, response+2, 3);
				uint64_t rfpower_int = atoi(rfpower);
				spe_update_rf_power(rfpower_int);
			} else {
				//Fixed power of 60 percent
				spe_update_rf_power(60);
			}
		}
		clear_response();
	}
}


int main()
{
	
	uint64_t current_tx_freq = 0;

	stdio_init_all();
	//Comment next below line when testing with wokwi
	//and uncomment when finished testing locally
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
		if (current_tx_freq != new_tx_freq) {
			current_tx_freq = new_tx_freq;
			spe_change_frequency(new_tx_freq);
		}
    }
}