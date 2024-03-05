// This is firmware for the Hermes Lite 2 IO board designed by Jim Ahlstrom, N2ADR. It is
//   Copyright (c) 2022-2023 James C. Ahlstrom <jahlstr@gmail.com>.
//   It is licensed under the MIT license. See MIT.txt.

// This is an interrupt service routine for I2C traffic. It must return quickly.

//#include "hl2ioboard.h"
//#include "i2c_registers.h"

uint8_t Registers[256];       // copy of registers written to the Pico
irq_handler IrqHandler[256];  // call these handlers (if any) after a register is written
//uint8_t rxEventBuffer[3];

uint64_t new_tx_freq;
uint8_t new_tx_fcode;
bool rx_freq_changed;
uint8_t rx_freq_high;
uint8_t rx_freq_low;
uint8_t i2c_regs_control;
uint8_t data, gpio, code1, code2;  //, fcode;
uint16_t adc;
uint8_t i2c_control_valid = false;  // is i2c_regs_control valid?
uint8_t inward[6];
#ifdef DEBUG_I2C
char RecStr[30] = "";
#endif

static void CheckHPF(void);

void receiveEvent(int byteCount) {
  //Receive and send I2C traffic. This is an interrupt service routine so return quickly!
  //The master will send two bytes, The first is the register, the second the data.
  int i;
#ifdef DEBUG_I2C  
  char numberString[4];

  strcat(RecStr, "Received: ");
#endif
  for (i = 0; i < byteCount; i++) {
    inward[i] = Wire1.read();
#ifdef DEBUG_I2C
    sprintf(numberString, "%d", static_cast<int>(inward[i]));
    strcat(RecStr, numberString);
    strcat(RecStr, ": ");
#endif
  }
#ifdef DEBUG_I2C
  inward[5] = byteCount;
  sprintf(numberString, "%d", static_cast<int>(inward[5]));
  strcat(RecStr, numberString);
#endif
  fast_led_flasher();

  i2c_regs_control = inward[0];
  if (byteCount == 1) {
    // This byte is a control register in preparation for a requestEvent so we are done
    return;
  }
  data = inward[1];

  if (i2c_regs_control >= GPIO_DIRECT_BASE && i2c_regs_control <= GPIO_DIRECT_BASE + 28) {  // direct write to a GPIO pin
                                                                                                 //   i2c_control_valid = false;
    Registers[i2c_regs_control] = data;
    gpio = i2c_regs_control - GPIO_DIRECT_BASE;
    if (gpio_get_function(gpio) == GPIO_FUNC_PWM)  // FAN_WRAP and FT817_WRAP are both equal to 1020
      pwm_set_gpio_level(gpio, (uint16_t)data * 4);
    else
      gpio_put(gpio, data);
    i2c_regs_control++;
  }
  else {
    Registers[i2c_regs_control] = data;  // this writes read-only registers too
    switch (i2c_regs_control) {
      case REG_CONTROL:
        if (data == 1) {  // perform a reset to power-up condition
          for (i = 0; i < 256; i++)
            Registers[i] = 0;
          new_tx_freq = 0;
          new_tx_fcode = 0;
          rx_freq_changed = false;
        }
        i2c_control_valid = false;  //debug*
        break;
      case REG_RF_INPUTS:  // How to use the external Rx input at J9
        switch (data) {
          case 0:  // Normal HL2 Rx input, J9 not used, Pure Signal at J10 available
            gpio_put(GPIO03_INTTR, 0);
            gpio_put(GPIO02_RF3, 0);
            break;
          case 1:  // Use J9 for Rx input, Pure Signal at J10 is not available
            gpio_put(GPIO03_INTTR, 1);
            gpio_put(GPIO02_RF3, 1);
            break;
            i2c_control_valid = false;
          case 2:  // Use J9 for Rx input on Rx, use Pure Signal at J10 for Tx
            gpio_put(GPIO03_INTTR, 1);
            IrqRxTxChange(GPIO13_EXTTR, 0xc);
            break;
        }
        break;
      case REG_FAN_SPEED:  // fan control
        pwm_set_chan_level(FAN_SLICE, FAN_CHAN, (uint16_t)data * 4);
        break;
      case REG_TX_FREQ_BYTE0:         // Tx frequency, LSB
        new_tx_freq = (uint64_t)data  // Thanks to Neil, G4BRK
                      | (uint64_t)Registers[REG_TX_FREQ_BYTE1] << 8
                      | (uint64_t)Registers[REG_TX_FREQ_BYTE2] << 16
                      | (uint64_t)Registers[REG_TX_FREQ_BYTE3] << 24
                      | (uint64_t)Registers[REG_TX_FREQ_BYTE4] << 32;
        new_tx_fcode = hertz2fcode(new_tx_freq);
        CheckHPF();

        i2c_control_valid = false;  //debug*

        break;
      case REG_FCODE_RX1:
      case REG_FCODE_RX2:
      case REG_FCODE_RX3:
      case REG_FCODE_RX4:
      case REG_FCODE_RX5:
      case REG_FCODE_RX6:
      case REG_FCODE_RX7:
      case REG_FCODE_RX8:
      case REG_FCODE_RX9:
      case REG_FCODE_RX10:
      case REG_FCODE_RX11:
      case REG_FCODE_RX12:
        code1 = code2 = 0;
        for (i = REG_FCODE_RX1; i <= REG_FCODE_RX12; i++) {
          fcode = Registers[i];
          if (fcode != 0) {
            if (fcode > code2)  // maximum fcode
              code2 = fcode;
            if (code1 == 0)  // minimum fcode
              code1 = fcode;
            else if (fcode < code1)
              code1 = fcode;
          }
        }
        rx_freq_low = code1;
        rx_freq_high = code2;
        rx_freq_changed = true;
        CheckHPF();


        i2c_control_valid = false;  //debug*

        break;
      case REG_OUT_PINS:
        if (gpio_get_function(GPIO08_Out8) == GPIO_FUNC_SIO)
          gpio_put(GPIO08_Out8, data & 0x80);
        gpio_put(GPIO09_Out7, data & 0x40);
        gpio_put(GPIO22_Out6, data & 0x20);
        gpio_put(GPIO10_Out5, data & 0x10);
        gpio_put(GPIO11_Out4, data & 0x08);
        gpio_put(GPIO20_Out3, data & 0x04);
        gpio_put(GPIO19_Out2, data & 0x02);
        if (gpio_get_function(GPIO16_Out1) == GPIO_FUNC_SIO)
          gpio_put(GPIO16_Out1, data & 0x01);
        break;
      case REG_STATUS:
        gpio_put(GPIO12_Sw5, data & 0x01);
        gpio_put(GPIO01_Sw12, data & 0x02);
        break;
    }
    if (IrqHandler[i2c_regs_control])
      (IrqHandler[i2c_regs_control])(i2c_regs_control, data);
    i2c_regs_control++;

    i2c_control_valid = false;  //debug*
  }
}



void requestEvent() {

#ifdef DEBUG_I2C
  strcat(RecStr, "At requestEvent: ");
  char numberString[4];  // Assuming maximum 3 digits for each number
  sprintf(numberString, "%d", static_cast<int>(Registers[i2c_regs_control]));
  strcat(RecStr, numberString);
  strcat(RecStr, ": ");
#endif

  data = 0;
  switch (i2c_regs_control) {
    case REG_FIRMWARE_MAJOR:
      data = firmware_version_major;
      break;
    case REG_FIRMWARE_MINOR:
      data = firmware_version_minor;
      break;
    case REG_IN_PINS:
      if (gpio_get_function(GPIO08_Out8) != GPIO_FUNC_SIO)
        data |= 0x80;
      if (gpio_get_function(GPIO16_Out1) != GPIO_FUNC_SIO)
        data |= 0x40;
      // fall through
    case REG_INPUT_PINS:  // return the state of the input pins
      if (gpio_get(GPIO06_In5))
        data |= 1 << 5;
      if (gpio_get(GPIO07_In4))
        data |= 1 << 4;
      if (gpio_get(GPIO21_In3))
        data |= 1 << 3;
      if (gpio_get(GPIO18_In2))
        data |= 1 << 2;
      if (gpio_get_function(GPIO17_In1) == GPIO_FUNC_SIO && gpio_get(GPIO17_In1))
        data |= 1 << 1;
      if (gpio_get(GPIO13_EXTTR))  // 1 for receive, 0 for transmit
        data |= 1;
      break;
    case REG_ADC0_MSB:  // perform an ADC conversion
      adc_select_input(0);
      adc = adc_read();
      Registers[i2c_regs_control] = data = adc >> 8;
      Registers[i2c_regs_control + 1] = adc & 0xFF;
      break;
    case REG_ADC1_MSB:
      adc_select_input(1);
      adc = adc_read();
      Registers[i2c_regs_control] = data = adc >> 8;
      Registers[i2c_regs_control + 1] = adc & 0xFF;
      break;
    case REG_ADC2_MSB:
      adc_select_input(2);
      adc = adc_read();
      Registers[i2c_regs_control] = data = adc >> 8;
      Registers[i2c_regs_control + 1] = adc & 0xFF;
      break;
    case REG_OUT_PINS:
      data = 0;
      if (gpio_get_function(GPIO08_Out8) == GPIO_FUNC_SIO && gpio_get(GPIO08_Out8))
        data |= 0x80;
      if (gpio_get(GPIO09_Out7))
        data |= 0x40;
      if (gpio_get(GPIO22_Out6))
        data |= 0x20;
      if (gpio_get(GPIO10_Out5))
        data |= 0x10;
      if (gpio_get(GPIO11_Out4))
        data |= 0x08;
      if (gpio_get(GPIO20_Out3))
        data |= 0x04;
      if (gpio_get(GPIO19_Out2))
        data |= 0x02;
      if (gpio_get_function(GPIO16_Out1) == GPIO_FUNC_SIO && gpio_get(GPIO16_Out1))
        data |= 0x01;
      break;
    case REG_STATUS:
      data = 0;
      if (gpio_get(GPIO12_Sw5))
        data |= 0x01;
      if (gpio_get(GPIO01_Sw12))
        data |= 0x02;
      if (gpio_get_function(GPIO17_In1) != GPIO_FUNC_SIO)
        data |= 0x04;
      break;
    default:
      if (i2c_regs_control >= GPIO_DIRECT_BASE && i2c_regs_control <= GPIO_DIRECT_BASE + 28) {  // direct read from a GPIO pin
        gpio = i2c_regs_control - GPIO_DIRECT_BASE;
        if (gpio_get_function(gpio) != GPIO_FUNC_SIO)
          data = Registers[i2c_regs_control];
        else if (gpio_get(gpio))
          data = 1;
        else
          data = 0;
      } else {
        data = Registers[i2c_regs_control];
      }
      break;
  }
  Wire1.write(data);
  i2c_regs_control++;
}


void IrqRxTxChange(uint gpio, uint32_t events) {  // Called when EXTTR changes

  if (Registers[REG_RF_INPUTS] == 2) {
    if (gpio_get(GPIO13_EXTTR))  // Rx
      gpio_put(GPIO02_RF3, 1);
    else  // Tx
      gpio_put(GPIO02_RF3, 0);
  }
  Serial.print("EXTTR has changed\n");
}

static void CheckHPF(void) {
  uint8_t fcode;

  if (rx_freq_low == 0)
    fcode = new_tx_fcode;
  else
    fcode = rx_freq_low;
  if (fcode <= 76)  // 2.55 MHz
    gpio_put(GPIO00_HPF, 0);
  else
    gpio_put(GPIO00_HPF, 1);
}