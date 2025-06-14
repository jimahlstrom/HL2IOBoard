// This is firmware for the Hermes Lite 2 IO board designed by Jim Ahlstrom, N2ADR. It is
//   Copyright (c) 2022-2023 James C. Ahlstrom <jahlstr@gmail.com>.
//   It is licensed under the MIT license. See MIT.txt.

// This firmware modified by Dalton Williams, W5EIM to do the following:
// Output BCD band code on J6 1-4

#include "../hl2ioboard.h"
#include "../i2c_registers.h"

// These are the major and minor version numbers for firmware. You must set these.
uint8_t firmware_version_major=1;
uint8_t firmware_version_minor=3;

int main()
{
	static uint8_t current_tx_fcode = 0;
	static bool current_is_rx = true;
	static uint8_t tx_band = 0;
	static uint8_t rx_band = 0;
	uint8_t band, fcode;
	bool is_rx;
	bool change_band;
	uint8_t i;

	stdio_init_all();
	configure_pins(false, true);
	configure_led_flasher();

	while (1) {					// Wait for something to happen
		sleep_ms(100);			// This sets the polling frequency.
		change_band = false;	// Initialize change_band

		// Poll for a changed Tx frequency.
		// The new_tx_fcode is set in the I2C handler.

		if (current_tx_fcode != new_tx_fcode) 
			{
			current_tx_fcode = new_tx_fcode;
			change_band = true;
			tx_band = fcode2band(current_tx_fcode);	// Convert the frequency code to a band code.
			}

		// Poll for a change in one of the twelve Rx frequencies. 
		// The rx_freq_changed is set in the I2C handler.

		if (rx_freq_changed) 
			{
			rx_freq_changed = false;
			change_band = true;
			}
		if (rx_freq_high == 0)
			{
			rx_band = tx_band;
			}
			else 
			{
			rx_band = fcode2band(rx_freq_high);	// Convert the frequency code to a band code.
			}

		// If the band has changed then set the BCD values for the band

		if (change_band)						// If the band has changed then set band
			{
			change_band = false;			
			is_rx = gpio_get(GPIO13_EXTTR);		// true for receive, false for transmit
			if (is_rx)
				{	
				band = rx_band;
				}
			else
				{	
				band = tx_band;
				}
			switch (band) {						// Set GPIO pins according to the band
				case BAND_160:
					gpio_put(GPIO16_Out1, 0);	//BCD Band Code D  J4 Pin 1 Gnd
					gpio_put(GPIO19_Out2, 0);	//BCD Band Code C  J4 Pin 2 Gnd
					gpio_put(GPIO20_Out3, 0);	//BCD Band Code B  J4 Pin 3 Gnd
					gpio_put(GPIO11_Out4, 1);	//BCD Band Code A  J4 Pin 4 +5v
					break;
				case BAND_80:
					gpio_put(GPIO16_Out1, 0);	//BCD Band Code D  J4 Pin 1 Gnd
					gpio_put(GPIO19_Out2, 0);	//BCD Band Code C  J4 Pin 2 Gnd
					gpio_put(GPIO20_Out3, 1);	//BCD Band Code B  J4 Pin 3 +5v
					gpio_put(GPIO11_Out4, 0);	//BCD Band Code A  J4 Pin 4 Gnd
					break;
				case BAND_60:
					gpio_put(GPIO16_Out1, 0);	//BCD Band Code D  J4 Pin 1 Gnd
					gpio_put(GPIO19_Out2, 0);	//BCD Band Code C  J4 Pin 2 Gnd
					gpio_put(GPIO20_Out3, 1);	//BCD Band Code B  J4 Pin 3 +5v
					gpio_put(GPIO11_Out4, 1);	//BCD Band Code A  J4 Pin 4 +5v
					break;
				case BAND_40:
					gpio_put(GPIO16_Out1, 0);	//BCD Band Code D  J4 Pin 1 Gnd
					gpio_put(GPIO19_Out2, 0);	//BCD Band Code C  J4 Pin 2 Gnd
					gpio_put(GPIO20_Out3, 1);	//BCD Band Code B  J4 Pin 3 +5v
					gpio_put(GPIO11_Out4, 1);	//BCD Band Code A  J4 Pin 4 +5v
					break;
				case BAND_30:
					gpio_put(GPIO16_Out1, 0);	//BCD Band Code D  J4 Pin 1 Gnd
					gpio_put(GPIO19_Out2, 1);	//BCD Band Code C  J4 Pin 2 +5v
					gpio_put(GPIO20_Out3, 0);	//BCD Band Code B  J4 Pin 3 Gnd	
					gpio_put(GPIO11_Out4, 0);	//BCD Band Code A  J4 Pin 4 Gnd
					break;
				case BAND_20:
					gpio_put(GPIO16_Out1, 0);	//BCD Band Code D  J4 Pin 1 Gnd	
					gpio_put(GPIO19_Out2, 1);	//BCD Band Code C  J4 Pin 2 +5v
					gpio_put(GPIO20_Out3, 0);	//BCD Band Code B  J4 Pin 3 Gnd
					gpio_put(GPIO11_Out4, 1);	//BCD Band Code A  J4 Pin 4 +5v
					break;
				case BAND_17:
					gpio_put(GPIO16_Out1, 0);	//BCD Band Code D  J4 Pin 1 Gnd
					gpio_put(GPIO19_Out2, 1);	//BCD Band Code C  J4 Pin 2 +5v
					gpio_put(GPIO20_Out3, 1);	//BCD Band Code B  J4 Pin 3 +5v
					gpio_put(GPIO11_Out4, 0);	//BCD Band Code A  J4 Pin 4 Gnd
					break;
				case BAND_15:
					gpio_put(GPIO16_Out1, 0);	//BCD Band Code D  J4 Pin 1 Gnd
					gpio_put(GPIO19_Out2, 1);	//BCD Band Code C  J4 Pin 2 +5v
					gpio_put(GPIO20_Out3, 1);	//BCD Band Code B  J4 Pin 3 +5v		
					gpio_put(GPIO11_Out4, 1);	//BCD Band Code A  J4 Pin 4 +5v
					break;
				case BAND_12:
					gpio_put(GPIO16_Out1, 1);	//BCD Band Code D  J4 Pin 1 +5v
					gpio_put(GPIO19_Out2, 0);	//BCD Band Code C  J4 Pin 2 Gnd	
					gpio_put(GPIO20_Out3, 0);	//BCD Band Code B  J4 Pin 3 Gnd
					gpio_put(GPIO11_Out4, 0);	//BCD Band Code A  J4 Pin 4 Gnd
					break;
				case BAND_10:
					gpio_put(GPIO16_Out1, 1);	//BCD Band Code D  J4 Pin 1 +5v	
					gpio_put(GPIO19_Out2, 0);	//BCD Band Code C  J4 Pin 2 Gnd
					gpio_put(GPIO20_Out3, 0);	//BCD Band Code B  J4 Pin 3 Gnd
					gpio_put(GPIO11_Out4, 1);	//BCD Band Code A  J4 Pin 4 +5v
					break;
				case BAND_6:
					gpio_put(GPIO16_Out1, 1);	//BCD Band Code D  J4 Pin 1 +5v
					gpio_put(GPIO19_Out2, 0);	//BCD Band Code C  J4 Pin 2 Gnd
					gpio_put(GPIO20_Out3, 1);	//BCD Band Code B  J4 Pin 3 +5v
					gpio_put(GPIO11_Out4, 0);	//BCD Band Code A  J4 Pin 4 Gnd
				break;
			}
		}
	}
}
