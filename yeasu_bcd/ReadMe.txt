This folder and software is an adaptation of software written by Jim Ahlstrom, N2ADR
for the Hermes Lite 2 IO board designed by Jim Ahlstrom, N2DDR.
Both are Copyright (c) 2022-2023 James C. Ahlstrom <jahlstr@gmail.com>.
And is licensed under the MIT license. See MIT.txt.

This adaptation is written by Dalton Williams, W5EIM to output BCD band code data
from the Hermes Lite 2 IO board to control amplifiers or tuners using the Yaesu
BCD band code protocol.

You should review the main.c file to understand how it works. Some knowledge of C++
is helpful but the code is well documented and explains how the BCD code is created.

The output is found on the J4 solder pads as follows:
     BCD Code A is on the J4 Pad 1 
     BCD Code B is on the J4 Pad 2 
     BCD Code C is on the J4 Pad 3 
     BCD Code D is on the J4 Pad 4
Solder jumper wires to the DB9 solder pads as appropriate for your equipment.

It is not necessary for you to compile the code.  You can simply copy the included
main.uf2 file to you IO board using these steps: 

     1. Power off the Hermes Lite 2 (HL2).
     2. Connect a USB cable to the I/O board's USB socket.
     3. Push and hold the button on the Pico microcontroller.
     4. Plug the USB cable into your PC while holding the Pico button. 
        The Pico will appear as a flash drive on your computer.
     5. Copy the main.uf2 file to the Pico drive.
     6. The Pico will reboot automatically after the file transfer.
     7. The Pico's LED should flash slowly indicating it is running correctly.
     8. You can then safely unplug the USB cable.
     9. Power on the HL2. 

If you wish to make modifications to the code, I used a Raspberry Pi 5, Pico SDK 
and pico development environment.

A description on how-to setup the pi pico development environment on a Raspberry Pi is here:
https://learn.arm.com/learning-paths/embedded-and-microcontrollers/rpi_pico/sdk/

After downloading the install script, follow the "Raspberry PIOS" instructions.

After you have verified the installation you can execute the following:
wget https://github.com/jimahlstrom/HL2IOBoard/archive/refs/heads/main.zip

Unzip:
unzip main.zip

Rename folder:
mv HL2IOBoard-main HL2IOBoard

Goto the w5eim_bcd folder:
cd /HL2IOBoard/yeasu_bcd

Delete the build folder
Create a new empty build folder

Goto build folder:
cd /HL2IOBoard/yeasu_bcd/build

Build application:
cmake ..
make

Copy main.uf2 to pico: [instructions above]

If you need help you can contact me by looking up my email address on QRZ
Best Wishes and 73
Dalton W5EIM


