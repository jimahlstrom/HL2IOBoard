# Arduino code conversion from Pico SDK

This firmware is the modification of the original Pico SDK firmware so that it will compile and install from the Arduino IDE. It is assumed that the user will have the Pico extension to the Arduino environment installed and is familiar with how to compile and upload. If this is not the case, there is plenty of information available on the internet and I recommend that you set it up and become familiar with it before proceeding.

## The General Concept

Apart from the i2c_slave_handler.c there is very little to change in the code. The Arduino IDE adds all the .ino files in the sketchbook directory into a single file and builds that so it is simply a matter of changing the name of the files xxx.c to xxx.ino, commenting out the hl2ioboard.h and i2c_registers.h includes and placing these slightly modified files into the sketchbook folder. Full details follow:

## The firmware example used

This is almostidentical to that used by Jim N2ADR in his n2adr_test example and uses the same libraries. There is a slight difference where Jim only switches 3 filters to show how to do the operation whereas this Arduino example switches five filters.

### The hardware and wiring required

The filter to be switched is a 1 KW capable home brew filter built as an outboard unit with four of its possible six filters installed. Ten metres is covered with an external 10 metre roofing filter in line so the ten metre position is simply a bridge although the future plan is to build the filter into that position. The 160 metre filter is not installed. The relays are activated by grounding the matching pin on the DB_9M plug on the front of the filter. The DB_9M plug connects to the DB_9M plug on the N2ADR IO Board via a DB_9F to DB_9F cable which is broken by inserting a ULN2003 to isolate the Pico IO Board and provide the drive current for the filter switching relays.

![My outboard LP Filter](./pictures/Filter.jpg)

* Solder a jumper from J4 pin 1 to J7 pin 2.
* Solder a jumper from J8 pin 1 to J7 pin 3.
* Solder a jumper from J12 pin 2 to J7 pin 5.

The Hardrock-50 shuts off its serial port while it's in transmit mode, which means there's no way to find out when it's done tuning. initially I used a timed transmit approach, but that had to allow for the longest possible tune cycle, so the transmitter stayed on longer than necessay. On the HR-50 mailing list, Patrick AC1KM suggested that it might be possible to use the transmitter's SWR reporting to monitor tuning state and Jim N2ADR suggested wiring that makes that possible. with the following connections, the firmware can monitor forward and reverse power and detect when tuning is complete. The only downside is that there aren't dedicated pads on the board, so you have to solder directly to pins on the 50-pin header and the Pico daughterboard. If you don't feel comfortable doing that, you can omit those connections. Tuning will still work, but the transmitter will stay on for a fixed period rather than shutting off when SWR has stopped changing.

### SWR monitoring connections

* Solder a jumper from J1 pin 37 to U1 pin 31.
* Solder a jumper from J1 pin 39 to U1 pin 32.



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