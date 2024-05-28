// This is firmware for the Hermes Lite 2 IO board designed by Jim Ahlstrom, N2ADR. It is
//   Copyright (c) 2022-2023 James C. Ahlstrom <jahlstr@gmail.com>.
//   It is licensed under the MIT license. See MIT.txt.

// This firmware outputs the FT817 band voltage on J4 pin 8 and sets the band.

#include <Wire.h>
#include "hl2ioboard.h"
#include "i2c_registers.h"

// These are the major and minor version numbers for firmware. You must set these.
uint8_t firmware_version_major = 7;
uint8_t firmware_version_minor = 9;

//#define DEBUG_I2C

static uint8_t current_tx_fcode = 0;
static bool current_is_rx = true;
static uint8_t tx_band = 0;
static uint8_t rx_band = 0;
uint8_t band, fcode;
bool is_rx;
bool change_band;
uint8_t i;

void setup() {
  configure_pins(false, true);
  configure_led_flasher();

  Wire1.begin(I2C1_ADDRESS);
  Wire1.onReceive(receiveEvent); // Set up a callback function for data receive
  Wire1.onRequest(requestEvent); // Set up a callback function for data request

#ifdef DEBUG_I2C
  Serial.begin(115200);  // send and receive at 115200 baud
#endif
}

void loop() {               // Wait for something to happen
  int input = 0;

  sleep_ms(1);  // This sets the polling frequency.
#ifdef DEBUG_I2C
  if(strlen(RecStr) > 0) {
    Serial.println(RecStr);
    strcpy(RecStr, "");
  }
#endif
  // Control the Icom AH-4 antenna tuner.
  // Assume the START line is on J4 pin 7 and the KEY line is on J8 pin 2.
  IcomAh4(GPIO22_Out7, GPIO18_In2);
  // Poll for a changed Tx band, Rx band and T/R change
  change_band = false;
  is_rx = gpio_get(GPIO13_EXTTR);  // true for receive, false for transmit
  if (current_is_rx != is_rx) {
    current_is_rx = is_rx;
    change_band = true;
  }
  // Poll for a changed Tx frequency. The new_tx_fcode is set in the I2C handler.
  if (current_tx_fcode != new_tx_fcode) {
    current_tx_fcode = new_tx_fcode;
    change_band = true;
    tx_band = fcode2band(current_tx_fcode);  // Convert the frequency code to a band code.
    ft817_band_volts(tx_band);               // Put the band voltage on J4 pin 8.
  }
  // Poll for a change in one of the twelve Rx frequencies. The rx_freq_changed is set in the I2C handler.
  if (rx_freq_changed) {
    rx_freq_changed = false;
    change_band = true;
    if (rx_freq_high == 0)
      rx_band = tx_band;
    else
      rx_band = fcode2band(rx_freq_high);  // Convert the frequency code to a band code.
  }
  if (change_band) {
    change_band = false;
    if (tx_band == 0)  // Tx band zero is a reset
      band = 0;
    else if (is_rx)
      band = rx_band;
    else
      band = tx_band;
    switch (band) {  // Set some GPIO pins according to the band
      case BAND_80:
        gpio_put(GPIO16_Out1, 1);
        gpio_put(GPIO19_Out2, 0);
        gpio_put(GPIO20_Out3, 0);
        gpio_put(GPIO11_Out4, 0);
        gpio_put(GPIO10_Out5, 0);
        gpio_put(GPIO22_Out6, 0);
        break;
      case BAND_40:
        gpio_put(GPIO16_Out1, 0);
        gpio_put(GPIO19_Out2, 1);
        gpio_put(GPIO20_Out3, 0);
        gpio_put(GPIO11_Out4, 0);
        gpio_put(GPIO10_Out5, 0);
        gpio_put(GPIO22_Out6, 0);
        break;
      case BAND_30:
        gpio_put(GPIO16_Out1, 0);
        gpio_put(GPIO19_Out2, 0);
        gpio_put(GPIO20_Out3, 1);
        gpio_put(GPIO11_Out4, 0);
        gpio_put(GPIO10_Out5, 0);
        gpio_put(GPIO22_Out6, 0);
        break;
      case BAND_20:
        gpio_put(GPIO16_Out1, 0);
        gpio_put(GPIO19_Out2, 0);
        gpio_put(GPIO20_Out3, 1);
        gpio_put(GPIO11_Out4, 0);
        gpio_put(GPIO10_Out5, 0);
        gpio_put(GPIO22_Out6, 0);
        break;
      case BAND_17:
        gpio_put(GPIO16_Out1, 0);
        gpio_put(GPIO19_Out2, 0);
        gpio_put(GPIO20_Out3, 0);
        gpio_put(GPIO11_Out4, 1);
        gpio_put(GPIO10_Out5, 0);
        gpio_put(GPIO22_Out6, 0);
        break;
      case BAND_15:
        gpio_put(GPIO16_Out1, 0);
        gpio_put(GPIO19_Out2, 0);
        gpio_put(GPIO20_Out3, 0);
        gpio_put(GPIO11_Out4, 1);
        gpio_put(GPIO10_Out5, 0);
        gpio_put(GPIO22_Out6, 0);
        break;
      case BAND_12:
        gpio_put(GPIO16_Out1, 0);
        gpio_put(GPIO19_Out2, 0);
        gpio_put(GPIO20_Out3, 0);
        gpio_put(GPIO11_Out4, 0);
        gpio_put(GPIO10_Out5, 1);
        gpio_put(GPIO22_Out6, 0);
        break;
      case BAND_10:
        gpio_put(GPIO16_Out1, 0);
        gpio_put(GPIO19_Out2, 0);
        gpio_put(GPIO20_Out3, 0);
        gpio_put(GPIO11_Out4, 0);
        gpio_put(GPIO10_Out5, 1);
        gpio_put(GPIO22_Out6, 0);
        break;
      default:  // This includes band zero (reset)
        gpio_put(GPIO16_Out1, 0);
        gpio_put(GPIO19_Out2, 0);
        gpio_put(GPIO20_Out3, 0);
        gpio_put(GPIO11_Out4, 0);
        gpio_put(GPIO10_Out5, 0);
        gpio_put(GPIO22_Out6, 0);
    }
  }
}

//************************* SUBROUTINES START HERE *************************//
