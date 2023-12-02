# IO Board Hardrock-50 Firmware by N1ADJ

This firmware uses the [Hardrock-50](https://hobbypcb.com/products/hardrock-50-hf-power-amp-kit) amplifier serial interface to do band changes and to trigger the built-in antenna tuner. The HR-50 serial protocol is described in Chapter 6 of the [Hardrock-50 Manual](https://sites.google.com/site/hardrock50beta/hardrock-50-builders-information-site/instruction-manual). The IO Board antenna tuning protocol is described in the [main README](../README.md#table-of-i2c-registers).

## Getting Started

### Wire your IO board for serial input/output

* Solder a jumper from J4 pin 1 to J7 pin 2.
* Solder a jumper from J8 pin 1 to J7 pin 3.
* Solder a jumper from J12 pin 2 to J7 pin 5.

![IO board wiring](./IOBoard.jpg)

### Install this firmware
* Power off the HL2 and connect a USB cable to the IO Board.
* Push the button on the Pico and then plug the USB cable into your PC.
* The Pico will appear as a flash drive on the PC. Then download the file [build/main.uf2](build/main.uf2) and copy it to the Pico.
* After the file is copied, the Pico will no longer show up as an external drive.
* Disconnect from the PC and power on the HL2.

For more detail, see the instructions in [Installing Firmware section of the main README](../README.md#installing-firmware).

### Amplifier setup

In the amplifier setup menu, set option `4. Transceiver` to `Other` and option `2. ACC Baud Rate` to 19200.

### Wire up a cable

You need a DB-9 male to DB-9 female cable with pins 2, 3 and 5 connected straight through, that is pin 2 to pin 2, pin 3 to pin 3 and pin 5 to pin 5.

## Operating

Once the cable is hooked between the IO Board and the HR-50, changing frequencies in SDR software that supports the IO Board should cause the amplifier to change bands. As you switch band in your software, you should see the amplifier change bands as well. It does _not_ trigger the antenna tuner&mdash;you must do that manually if needed.

If you tune to a frequency outside of a band the amplifier supports, the HR-50 `BAND:` display will indicate `UNK` and the amplifier will not go into transmit. Because the IO Board doesn't receive the exact transmit frquency, this will not happen exactly at the band edge, but when tuned some distance outside of it. It should not happen when tuned inside the band.

As far as I'm aware, the only software that implements the antenna tuning protocol is [Reid's HL2-specific Thetis fork](https://github.com/mi0bot/OpenHPSDR-Thetis/releases), starting in version v2.10.3.4-beta1. To trigger automatic tuning, click the `TUNE` button while pressing the `Ctrl` key. (Be sure the `Setup|General|Ant/Filters|Antenna|Disable on Tune Pwr <35W` box is checked, or transmit may stop before tuning is complete.)

## Questions?

Please post questions, issues, etc. to the [Hermes-Lite group](https://groups.google.com/g/hermes-lite).
