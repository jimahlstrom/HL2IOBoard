// This is firmware for the Hermes Lite 2 IO board designed by Jim Ahlstrom, N2ADR. It is
//   Copyright (c) 2022-2023 James C. Ahlstrom <jahlstr@gmail.com>.
//   It is licensed under the MIT license. See MIT.txt.

// This version is for K3-compatible band data output by Neil Whiting, G4BRK

#include "../hl2ioboard.h"
#include "../i2c_registers.h"

// These are the major and minor version numbers for firmware. You must set these.
uint8_t firmware_version_major=1;
uint8_t firmware_version_minor=3;

// This function for K3-compatible band data output by Neil Whiting, G4BRK

// Output data on connector J4, J6 according to the band.
// Pin details and wiring:
// K3 signal  HL2 GPIO      HL2IO    HL2 ACC
// GND                      J4  1    1
// band0 out  GPIO16_Out1   J4  1    2
// band1 out  GPIO19_Out2   J4  2    3
// band2 out  GPIO20_Out3   J4  3    4
// band3 out  GPIO11_Out4   J4  4    5
// digout0    GPIO10_Out5   J6  5    6
// Keyout-lp  GPIO22_Out6   J6  6    7
// 

void k3_band_data(uint8_t fcode)	// 
// Band    160     80     60     40     30     20     17     15     12     10      6      4      2   70cm   23cm   13cm    9cm    6cm    3cm
// BandData  1      2      0      3      4      5      6      7      8      9      1      2      3      4      5      6      7      8      9
// Digout0   0      0      0      0      0      0      0      0      0      0      1      1      1      1      1      1      1      1      1
{

	static uint8_t banddata = 0;
	static uint8_t digout0  = 0;
	
	switch (fcode) {
	    case BAND_160:     banddata =  1; digout0  = 0; break;
	    case BAND_80:      banddata =  2; digout0  = 0; break;        
	    case BAND_60:      banddata =  0; digout0  = 0; break;
	    case BAND_40:      banddata =  3; digout0  = 0; break;        
	    case BAND_30:      banddata =  4; digout0  = 0; break;        
	    case BAND_20:      banddata =  5; digout0  = 0; break;        
	    case BAND_17:      banddata =  6; digout0  = 0; break;        
	    case BAND_15:      banddata =  7; digout0  = 0; break;        
	    case BAND_12:      banddata =  8; digout0  = 0; break;        
	    case BAND_10:      banddata =  9; digout0  = 0; break;        
	    case BAND_6:       banddata =  1; digout0  = 1; break;        
	    case BAND_4:       banddata =  2; digout0  = 1; break;        
	    case BAND_2:       banddata =  3; digout0  = 1; break;        
	    case BAND_70cm:    banddata =  4; digout0  = 1; break;
	    case BAND_23cm:    banddata =  5; digout0  = 1; break;
	    case BAND_13cm:    banddata =  6; digout0  = 1; break;
	    case BAND_9cm:     banddata =  7; digout0  = 1; break;
	    case BAND_5cm:     banddata =  8; digout0  = 1; break;
	    case BAND_3cm:     banddata =  9; digout0  = 1; break;
	    default:           banddata = 11; digout0  = 0; break;
	}
	gpio_put(GPIO10_Out5, digout0  & 0x01);
	gpio_put(GPIO16_Out1, banddata & 0x01);
	gpio_put(GPIO19_Out2, banddata & 0x02);
	gpio_put(GPIO20_Out3, banddata & 0x04);
	gpio_put(GPIO11_Out4, banddata & 0x08);
}

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
    
	while (1) {	// Wait for something to happen
		sleep_ms(1);	// This sets the polling frequency.
		// Control the Icom AH-4 antenna tuner.
		// Assume the START line is on J4 pin 6 and the KEY line is on J8 pin 2.
		IcomAh4(GPIO22_Out6, GPIO18_In2);
		// Poll for a changed Tx band, Rx band and T/R change
		change_band = false;
		is_rx = gpio_get(GPIO13_EXTTR);		// true for receive, false for transmit
		if (current_is_rx != is_rx) {
			current_is_rx = is_rx;
			change_band = true;
		}
		// Poll for a changed Tx frequency. The new_tx_fcode is set in the I2C handler.
		if (current_tx_fcode != new_tx_fcode) {
			current_tx_fcode = new_tx_fcode;
			change_band = true;
			tx_band = fcode2band(current_tx_fcode);		// Convert the frequency code to a band code.
//			ft817_band_volts(tx_band);			        // Put the band voltage on J4 pin 8.
			k3_band_data(tx_band);			            // Put the K3 band data on J4/6.
//            printf("NewTXFreq = %d NewTXCode = %d\n", new_tx_fcode, tx_band);
		}
		// Poll for a change in one of the twelve Rx frequencies. The rx_freq_changed is set in the I2C handler.
		if (rx_freq_changed) {
			rx_freq_changed = false;
			change_band = true;
			fcode = 0;
			for (i = REG_FCODE_RX1; i <= REG_FCODE_RX12; i++) {	// find the highest Rx frequency
				if (Registers[i] > fcode)
					fcode = Registers[i];
			}
			rx_band = fcode2band(fcode);		// Convert the frequency code to a band code.
			if (rx_band == 0)
				rx_band = tx_band;
		}
		if (change_band) {
			change_band = false;
			if (tx_band == 0)	// Tx band zero is a reset
				band = 0;
			else if (is_rx)
				band = rx_band;
			else
				band = tx_band;

		}
	}
}
