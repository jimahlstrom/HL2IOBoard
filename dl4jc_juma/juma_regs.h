// I2C register block for the JUMA firmware on the Hermes Lite 2 IO board.
//
// The board's own registers are defined in the HL2IOBoard project
// (i2c_registers.h) and occupy 0..32 and 167..198. Everything in between is
// free, so this firmware puts its own block at 0x40 - far enough from the
// upstream range that new board registers will not collide with it.
//
// A read from the Pico returns four bytes starting at the address, so values
// that belong together are kept adjacent.
#pragma once

// --- Being found by the SDR software --------------------------------------
// Some SDR software will not send the transmit frequency to a board it has not
// recognised. deskHPSDR looks for the PCA9536 at I2C 0x41 and expects four
// bytes of 0xF1, which a real IO board answers in hardware; failing that it
// reads register 33 from the Pico at 0x1D and expects 0xEF.
//
// On an IO board this answer changes nothing - 0x41 has already done the job.
// It is here for a board built without the PCA9536, which is what the comment
// beside that code has in mind: "so you can use N2ADRs firmware code base for
// your own projects without buying the IO Board".
//
// Register 34 is read afterwards as a low-pass filter bitmask and stays zero.
// That is correct rather than missing: the filter board is switched by the
// HL2's own gateware, and the Pico cannot read its state.
#define REG_LPF_DETECT      33    // read as 0xEF -> "a Pico is here"
#define REG_LPF_STATUS      34    // filter bitmask; none here, so 0
#define LPF_DETECT_MAGIC    0xEF

#define REG_JUMA_MODE       0x40  // 64  rw  standing behaviour, see below
#define REG_JUMA_CMD        0x41  // 65  rw  one-shot command, cleared when sent
#define REG_JUMA_LINK       0x42  // 66  ro  1 = the status below is fresh
#define REG_JUMA_FLAGS      0x43  // 67  ro  state bits, see below

#define REG_JUMA_BAND       0x44  // 68  ro  band the PA reports: 1..9, 10 = unknown
#define REG_JUMA_WANT_BAND  0x45  // 69  ro  band this firmware wants, 0 = none
#define REG_JUMA_GAIN       0x46  // 70  ro  attenuator 1..4 (G1 = 6 dB ... G4 = 0 dB)
#define REG_JUMA_ALARMS     0x47  // 71  ro  alarm bits as the PA reports them

#define REG_JUMA_SWR        0x48  // 72  ro  VSWR x 10       (1.4 -> 14)
#define REG_JUMA_VOLTS      0x49  // 73  ro  supply volts x 10 (13.6 -> 136)
#define REG_JUMA_AMPS       0x4A  // 74  ro  current, amps x 10
#define REG_JUMA_TEMP       0x4B  // 75  ro  temperature, two's complement, scale per FLAGS bit 3

#define REG_JUMA_WATTS_MSB  0x4C  // 76  ro  output power in watts, 16 bit
#define REG_JUMA_WATTS_LSB  0x4D  // 77  ro
#define REG_JUMA_FAN        0x4E  // 78  ro  0 off, 1 slow, 2 medium, 3 fast
#define REG_JUMA_REPLIES    0x4F  // 79  ro  status replies, low byte - counts up while the link lives

#define REG_JUMA_BADLINES   0x50  // 80  ro  lines that were no status reply, low byte
#define REG_JUMA_SET_GAIN   0x51  // 81  rw  write 1..4 -> '=Gn', cleared when sent
#define REG_JUMA_SET_BAND   0x52  // 82  rw  write 1..9 -> '=Bn', cleared when sent
#define REG_JUMA_LOST       0x53  // 83  ro  bytes dropped by a full receive buffer, low byte

// The same measurements again, at the precision the PA itself reports. The bytes
// above keep tenths so that one four byte read covers the lot; these are for a
// display that wants to show 13.66 V rather than 13.6.
#define REG_JUMA_VOLTS100_MSB 0x54  // 84  ro  volts x 100, 16 bit
#define REG_JUMA_VOLTS100_LSB 0x55  // 85  ro
#define REG_JUMA_AMPS100_MSB  0x56  // 86  ro  amps x 100, 16 bit
#define REG_JUMA_AMPS100_LSB  0x57  // 87  ro
#define REG_JUMA_WATTS10_MSB  0x58  // 88  ro  watts x 10, 16 bit
#define REG_JUMA_WATTS10_LSB  0x59  // 89  ro
#define REG_JUMA_SWR100_MSB   0x5A  // 90  ro  VSWR x 100, 16 bit
#define REG_JUMA_SWR100_LSB   0x5B  // 91  ro

// The power-up banner of the amplifier, a character at a time: write the index
// to the first, read the character from the second. Index 0 gives the length.
// This is what says which firmware is in the PA - an original, a clone, or
// something nobody here has tried.
#define REG_JUMA_BANNER_IDX 0x5C  // 92  rw  which character to look at
#define REG_JUMA_BANNER_CH  0x5D  // 93  ro  that character, or the length at 0

#define REG_JUMA_SAVED      0x5E  // 94  ro  1 = the mode below came out of flash

// --- A snapshot that can be checked ---------------------------------------
// The HL2's I2C bridge silently drops a command that arrives while it is busy
// (gateware, i2c_bus2.v: "// Missed"), and cmd_resp_data then still holds the
// PREVIOUS read's four bytes. The core does not even wire up the bridge's
// acknowledge line (hermeslite_core.v: ".cmd_ack() // No need for ack"), so a
// host has no way of being told. Measured here: about 4 % of rounds come back
// shifted, and no amount of waiting removes it.
//
// So the data carries its own proof. The host writes REG_JUMA_SNAP, which makes
// the firmware copy the current values into the block below and stamp every four
// byte group with
//
//     stamp = (generation << 3) | group number
//
// Both halves are needed. The generation catches a group left over from an
// earlier snapshot. The group number catches the other case, which a single
// shared tag would let through: a dropped read of group B hands back group A,
// and if the stamp were only a generation it would match. The host checks that
// every group carries its own number and that all generations agree, then that
// the generation has moved on since last time.
#define REG_JUMA_SNAP       0x5F  // 95  wo  write anything -> take a snapshot

#define REG_SNAP_A          0x60  // tag, link, flags, band
#define REG_SNAP_B          0x64  // tag, want_band, gain, alarms
#define REG_SNAP_C          0x68  // tag, volts x100 MSB, LSB, temp
#define REG_SNAP_D          0x6C  // tag, amps x100 MSB, LSB, fan
#define REG_SNAP_E          0x70  // tag, watts x10 MSB, LSB, mode
#define REG_SNAP_F          0x74  // tag, VSWR x100 MSB, LSB, fault
#define REG_SNAP_G          0x78  // tag, replies, badlines, lost
#define REG_SNAP_GROUPS     7
#define SNAP_GEN(stamp)     ((stamp) >> 3)
#define SNAP_GROUP(stamp)   ((stamp) & 0x07)
// The generation runs 1..31, never 0, so a block nobody has written cannot pass
// for a snapshot.

// REG_JUMA_MODE - standing behaviour. All bits clear (the power-on state) is
// the intended normal case: follow the transmit frequency, touch nothing else.
#define JUMA_MODE_NO_BAND      0x01  // do NOT send '=Bn' on a frequency change
#define JUMA_MODE_HOLD_OPERATE 0x02  // put the PA back into OPERATE when it falls to STANDBY
#define JUMA_MODE_TELEMETRY    0x04  // print one status line per second on the USB port
#define JUMA_MODE_PROXY        0x08  // the USB port becomes the PA's serial port, verbatim

// REG_JUMA_CMD - written by the host, cleared by the firmware once the command
// has gone out. The two power-off codes are deliberately far away from the
// small numbers so that a host walking through register values cannot switch
// the amplifier off by accident.
#define JUMA_CMD_OPERATE    1     // '=O'
#define JUMA_CMD_STANDBY    2     // '=S'
#define JUMA_CMD_AUTO_BAND  3     // '=A' - hand band selection back to the PA
#define JUMA_CMD_CLR_ALARM  4     // '=C'
#define JUMA_CMD_POWER_OFF  0xA0  // '=P0' - switch the PA off, discard its state
#define JUMA_CMD_POWER_SAVE 0xA1  // '=P1' - switch the PA off, save its state

// REG_JUMA_FLAGS
#define JUMA_FLAG_OPERATE   0x01  // PA is in OPERATE (status field 1)
#define JUMA_FLAG_AUTO_SEL  0x02  // PA does its own band selection (field 2)
#define JUMA_FLAG_PA_TX     0x04  // PA says it is transmitting (field 3)
#define JUMA_FLAG_CELSIUS   0x08  // temperature in C, else F (field 4)
#define JUMA_FLAG_FOLLOWING 0x10  // band following is switched on
#define JUMA_FLAG_HL2_TX    0x20  // EXTTR from the HL2 says transmit

// REG_FAULT (register 8, defined by the board) - this firmware uses it as a
// bit field.
#define JUMA_FAULT_NO_LINK  0x01  // no status reply from the PA
#define JUMA_FAULT_ALARM    0x02  // the PA has an alarm latched
#define JUMA_FAULT_BAD_CMD  0x04  // a register was written with a value it cannot take
