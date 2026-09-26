// JUMA amplifier firmware for the Hermes Lite 2 IO board by N2ADR.
//
//   Copyright (c) 2022-2023 James C. Ahlstrom <jahlstr@gmail.com>  (the board
//     and its library; this file follows the structure of his examples)
//   Copyright (c) 2026 DL4JC <mail@dl4jc.de>  (the JUMA part)
//   MIT licence, both parts - see MIT.txt in the HL2IOBoard project and
//   LICENSE in this one.
//
// The board sends the transmit frequency to the Pico over I2C. This firmware
// turns that into a JUMA band command over the serial port, and reads the PA's
// status back into I2C registers so the SDR software can see what the
// amplifier is doing.
//
// The path is the same one M0HPF's SPE firmware uses - Pico UART, a MAX3232 to
// get RS-232 levels, and the amplifier's own serial command set. Only the
// commands differ, and the JUMA differs in three ways that shape this file:
//
//   1. It is silent until asked. The SPE polls the transceiver; the JUMA
//      answers '=R' and says nothing otherwise. So we are the master and must
//      poll, not wait.
//   2. Remote mode times out. 5 s after the last command the PA leaves remote
//      mode and drops to STANDBY (manual, annex D). The 500 ms poll is what
//      keeps it alive - it is not just there for the display.
//   3. '=Bn' is a *manual* band selection. It moves the PA from A to M, and it
//      can knock the PA out of OPERATE. See hold_operate() below.
//
// The status parser and the band table are not duplicated here: they are the
// same files the ESP32 firmware in this repository uses, so a corrected band
// edge or a corrected field is corrected for both. tests/run.sh covers them on
// the host.
// The board's library is C, this file is C++ - the parser and the band table
// are shared with the ESP32 firmware and that is C++. So hl2ioboard.h has to
// be declared with C linkage, or its functions would be looked up under
// mangled names and nothing would link. The SDK headers come first, before
// that wrapper: they carry their own linkage guards, and their include guards
// then turn the nested includes inside hl2ioboard.h into no-ops.
#include <hardware/gpio.h>
#include <hardware/i2c.h>
#include <hardware/pwm.h>
#include <hardware/adc.h>
#include <hardware/uart.h>
#include <hardware/flash.h>
#include <hardware/sync.h>
#include <pico/i2c_slave.h>
#include <pico/binary_info.h>
#include <pico/stdlib.h>
#include <stdio.h>
#include <string.h>

extern "C" {
#include <hl2ioboard.h>
}
#include <i2c_registers.h>

#include "juma_regs.h"
#include "juma_status.h"
#include "bands.h"

// The UART on J4 pin 1 (TX) and J8 pin 1 (RX). configure_pins(true, ...)
// switches those pins to the UART, which on the RP2040 is uart0.
#define UART_ID     uart0
#define BAUD_RATE   115200        // the PA-100D speaks 115200 8N1
#define DATA_BITS   8
#define STOP_BITS   1
#define PARITY      UART_PARITY_NONE

// Prints the traffic to the USB serial port and accepts typed commands there.
// Off for normal use. Unlike the other firmwares for this board this is not
// edited in the source but set at configure time:
//     cmake -DJUMA_DEBUG=ON ..
// so the quiet image and the talking one can be built from the same tree.
#ifndef JUMA_DEBUG
#define JUMA_DEBUG  0
#endif

// What REG_JUMA_MODE holds at power-up, and again after the host resets the
// registers. Every register comes up zero, which is right for band following
// but leaves the OPERATE hold off - and on a board with no software that knows
// these registers, nobody is there to switch it on. So it can be built in:
//     cmake -DJUMA_HOLD_OPERATE=ON ..
#ifndef JUMA_MODE_AT_BOOT
#define JUMA_MODE_AT_BOOT 0
#endif

// Major and minor firmware version, returned in REG_FIRMWARE_MAJOR/MINOR.
uint8_t firmware_version_major = 1;
uint8_t firmware_version_minor = 0;

// --- Timing ---------------------------------------------------------------
// Remote mode dies 5 s after the last command, so the poll has to be well
// inside that. The manual suggests about 1 s; 500 ms costs nothing here.
static const uint32_t POLL_INTERVAL_MS = 500;
// After this long without an understood status reply the link counts as dead.
static const uint32_t STALE_AFTER_MS   = 3000;
// Minimum spacing between two commands to the PA.
static const uint32_t CMD_GAP_MS       = 40;
// The frequency must hold still this long before a band command goes out.
// Tuning across a band edge would otherwise fire one command per step - and
// it is also what makes a torn read of the 64-bit new_tx_freq harmless: a
// value that the I2C interrupt was in the middle of writing is replaced on the
// next pass, 1 ms later, and so never survives to become a band command.
static const uint32_t BAND_SETTLE_MS   = 150;
// Do not repeat a band command faster than this.
static const uint32_t BAND_REPEAT_MS   = 1000;
// Spacing and count of the attempts to push the PA back where we want it.
// Bounded, so a choice somebody made at the front panel is not overridden for
// ever - it gives way after three tries and reports through REG_JUMA_FLAGS.
static const uint32_t RETRY_GAP_MS     = 5000;
static const uint8_t  RETRY_MAX        = 3;

static inline uint32_t now_ms(void)
{
	return to_ms_since_boot(get_absolute_time());
}

// --- Remembering the mode -------------------------------------------------
// The registers are RAM and come up zero, so without this a mode the operator
// set - the OPERATE hold above all - would be gone at the next power-up, and
// getting it back would mean opening the box to reflash. The last flash sector
// holds it instead.
//
// Writing flash means erasing a whole sector with interrupts off, which takes
// tens of milliseconds. During that the UART interrupt does not run and the
// receive FIFO (32 bytes, 2.7 ms at 115200) overflows, so a status line is lost.
// That is why it only happens when the mode has really changed, has then stood
// still for a while, and the PA is not transmitting.
#define SAVE_MAGIC   0x4A554D41u        // 'JUMA'
#define SAVE_OFFSET  (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
static const uint32_t SAVE_SETTLE_MS = 3000;

struct Saved {
	uint32_t magic;
	uint8_t  version;
	uint8_t  mode;
	uint8_t  pad[2];
	uint32_t check;                 // magic ^ version ^ mode, so a half
};                                      // written page cannot read as valid

static uint8_t  saved_mode   = 0;       // what is in flash now
static bool     saved_valid  = false;
static uint32_t mode_changed_at = 0;
static uint8_t  mode_last    = 0;

static bool save_load(uint8_t *mode)
{
	const Saved *f = (const Saved *)(XIP_BASE + SAVE_OFFSET);
	if (f->magic != SAVE_MAGIC || f->version != 1)
		return false;
	if (f->check != (SAVE_MAGIC ^ f->version ^ f->mode))
		return false;
	*mode = f->mode;
	return true;
}

static void save_store(uint8_t mode)
{
	// A whole page, because that is the smallest unit flash_range_program
	// takes, and it has to be aligned - hence the static buffer.
	static uint8_t page[FLASH_PAGE_SIZE] __attribute__((aligned(4)));
	Saved *rec = (Saved *)page;

	memset(page, 0xFF, sizeof(page));
	rec->magic   = SAVE_MAGIC;
	rec->version = 1;
	rec->mode    = mode;
	rec->pad[0]  = rec->pad[1] = 0;
	rec->check   = SAVE_MAGIC ^ 1u ^ mode;

	uint32_t ints = save_and_disable_interrupts();
	flash_range_erase(SAVE_OFFSET, FLASH_SECTOR_SIZE);
	flash_range_program(SAVE_OFFSET, page, FLASH_PAGE_SIZE);
	restore_interrupts(ints);

	saved_mode  = mode;
	saved_valid = true;
}

// --- Receiving ------------------------------------------------------------
// The interrupt handler only moves bytes into a ring buffer; the lines are
// assembled and parsed in the main loop. Parsing in the handler would mean
// doing float conversions with interrupts off, and the board's rule is that
// an interrupt service routine returns quickly.
#define RX_RING 512
static volatile uint8_t  rx_ring[RX_RING];
static volatile uint16_t rx_head = 0, rx_tail = 0;   // head: handler, tail: loop
static volatile uint16_t rx_lost = 0;

static void on_uart_rx(void)
{
	while (uart_is_readable(UART_ID)) {
		uint8_t ch = uart_getc(UART_ID);
		uint16_t next = (uint16_t)((rx_head + 1) % RX_RING);
		if (next == rx_tail) {		// buffer full: drop, and say so
			rx_lost++;
			continue;
		}
		rx_ring[rx_head] = ch;
		rx_head = next;
	}
}

// --- State ----------------------------------------------------------------
static JumaStatus st;			// what the PA last told us
static uint32_t   last_rx_ms   = 0;	// when that was
static uint32_t   replies      = 0;	// understood status replies
static uint32_t   bad_lines    = 0;	// everything else, banner included
static char       line[128];
static uint8_t    line_len     = 0;
static char       banner[48]   = {0};   // what the PA called itself at power-up
static uint8_t    banner_len   = 0;

static uint64_t   tx_freq      = 0;	// transmit frequency we are acting on
static uint32_t   freq_set_at  = 0;	// when it last changed
static uint8_t    want_band    = 0;	// band that frequency asks for

static uint32_t   last_band_cmd = 0;
static uint8_t    force_m_tries = 0;	// attempts to pull the PA from A to M
static uint32_t   last_force_m  = 0;
static uint8_t    operate_tries = 0;	// attempts to get OPERATE back
static uint32_t   last_operate  = 0;

// REG_CONTROL == 1 resets the board to its power-up state. The library does that
// inside the I2C interrupt, so all we do here is note it and act in the loop -
// the register handler has to return quickly.
static volatile bool host_reset = false;

static void on_control_written(uint8_t reg, uint8_t data)
{
	(void)reg;
	if (data == 1)
		host_reset = true;
}

static bool online(void)
{
	return st.valid && (now_ms() - last_rx_ms) < STALE_AFTER_MS;
}

// Either side saying "transmit" is enough. EXTTR from the HL2 leads the RF and
// is the earlier of the two; the PA's own field 3 covers a transmission the
// HL2 knows nothing about, for instance during a tune-up from another rig.
static bool transmitting(void)
{
	return !gpio_get(GPIO13_EXTTR) || (online() && st.tx);
}

// --- Sending --------------------------------------------------------------
// A short queue rather than writing straight to the UART: a band change, an
// '=O' behind it and the running poll would otherwise arrive back to back, and
// the PA wants a gap between commands.
#define QN 8
#define QL 8
static char    q[QN][QL];
static uint8_t q_head = 0, q_tail = 0;
static uint32_t last_tx_ms = 0;
static char    last_sent[QL] = "";

// Commands and replies end with 0x0A 0x0D - LF before CR, not the usual way
// round. The manual writes it as "=[Command][Parameter]\n\r".
static const char *TERM = "\n\r";

static bool enqueue(const char *cmd)
{
	// Refuse rather than truncate. The longest thing the PA takes is '=P0',
	// so anything approaching QL is a typo on the console - and half of a
	// mistyped command is worse than none of it.
	if (strlen(cmd) >= QL)
		return false;
	uint8_t next = (uint8_t)((q_tail + 1) % QN);
	if (next == q_head)
		return false;			// full; the caller retries next pass
	strcpy(q[q_tail], cmd);
	q_tail = next;
	return true;
}

static void pump_queue(void)
{
	if (q_head == q_tail)
		return;
	if (now_ms() - last_tx_ms < CMD_GAP_MS)
		return;
	uart_puts(UART_ID, q[q_head]);
	uart_puts(UART_ID, TERM);
#if JUMA_DEBUG
	printf("-> %s\n", q[q_head]);
#endif
	snprintf(last_sent, sizeof(last_sent), "%s", q[q_head]);
	q_head = (uint8_t)((q_head + 1) % QN);
	last_tx_ms = now_ms();
}

static bool set_band(uint8_t n)
{
	if (n < 1 || n > 9)
		return false;
	char c[4] = { '=', 'B', (char)('0' + n), '\0' };
	return enqueue(c);
}

static bool set_gain(uint8_t n)
{
	if (n < 1 || n > 4)
		return false;
	char c[4] = { '=', 'G', (char)('0' + n), '\0' };
	return enqueue(c);
}

// --- Parsing --------------------------------------------------------------
static void parse_line(char *s)
{
#if JUMA_DEBUG
	const bool    had     = st.valid;
	const bool    was_op  = st.operate;
	const bool    was_sel = st.autoSel;
	const uint8_t was_bnd = st.band;
#endif
	if (jumaParseStatus(s, st)) {
		last_rx_ms = now_ms();
		replies++;
#if JUMA_DEBUG
		// The state changes are worth more than the raw lines: a STANDBY
		// directly behind a '=Bn' is the PA dropping out on a band change,
		// not somebody at the front panel.
		if (had) {
			if (was_op != st.operate)
				printf("pa: %s -> %s (band %u, sel %c, last sent '%s')\n",
					was_op ? "OPERATE" : "STANDBY",
					st.operate ? "OPERATE" : "STANDBY",
					st.band, st.autoSel ? 'A' : 'M', last_sent);
			if (was_bnd != st.band)
				printf("pa: band %u -> %u (sel %c)\n",
					was_bnd, st.band, st.autoSel ? 'A' : 'M');
			if (was_sel != st.autoSel)
				printf("pa: band select %c -> %c\n",
					was_sel ? 'A' : 'M', st.autoSel ? 'A' : 'M');
		}
		printf("<- %s\n", st.raw);
#endif
		return;
	}
	// A byte lost to a full ring buffer corrupts the line it was part of, so
	// it arrives here as well - the host sees one number, not two.
	bad_lines++;

	// Keep it if it reads as text. Power-up banners look like
	// "Juma PA-100D V4.00a"; a wrong baud rate produces bytes that do not,
	// and those must not be mistaken for one.
	uint8_t printable = 0, total = 0;
	for (const char *c = s; *c && total < sizeof(banner) - 1; c++, total++)
		if (*c >= 32 && *c < 127)
			printable++;
	if (total >= 4 && printable == total) {
		memcpy(banner, s, total);
		banner[total] = '\0';
		banner_len = total;
	}
#if JUMA_DEBUG
	// Typically the power-up banner, "Juma PA-100D V4.00a" or whatever the
	// firmware in the amplifier calls itself. At the wrong baud rate this is
	// where the rubbish shows up.
	printf("?? %s  (dropped %u)\n", s, (unsigned)rx_lost);
#endif
}

// --- The USB port as the PA's serial port ---------------------------------
// With JUMA_MODE_PROXY set, everything the PA says goes out on USB byte for byte
// and everything arriving on USB goes to the PA. A program on the PC then speaks
// the JUMA command set out of the manual and needs to know nothing about this
// board - which also means the ESP32 firmware's own tooling works against it.
//
// Two things a caller has to know. The firmware keeps polling '=R' twice a
// second, because that is what holds the PA in remote mode, so status lines
// arrive unasked; and a caller that wants the band has to switch band following
// off first, see submit_from_usb() below.
//
// stdio_put_string() with cr_translation false, not printf: the SDK turns '\n'
// into "\r\n" by default, which would corrupt the PA's "\n\r" terminator.
static inline bool proxy_on(void)
{
	return (Registers[REG_JUMA_MODE] & JUMA_MODE_PROXY) != 0;
}

static void usb_raw(const char *s, int len)
{
	stdio_put_string(s, len, false, false);
}

static void drain_rx(void)
{
	while (rx_tail != rx_head) {
		char c = (char)rx_ring[rx_tail];
		rx_tail = (uint16_t)((rx_tail + 1) % RX_RING);
		// Every CR and every LF ends a line, and empty lines are dropped.
		// Measured on a PA-100D the terminator is three bytes, 0D 0A 0D,
		// not the two the manual documents - matching "\n\r" exactly only
		// works as long as the surplus byte happens to land harmlessly.
		if (c == '\n' || c == '\r') {
			if (line_len > 0) {
				line[line_len] = '\0';
				// Before parse_line(): the parser cuts the line up in
				// place, replacing every ':' with a terminator.
				if (proxy_on()) {
					usb_raw(line, (int)line_len);
					usb_raw(TERM, 2);
				}
				parse_line(line);
				line_len = 0;
			}
			continue;
		}
		if (line_len < sizeof(line) - 1)
			line[line_len++] = c;
		else
			line_len = 0;		// overrun: throw the line away
	}
}

// --- Band following -------------------------------------------------------
static void band_control(void)
{
	// The 64-bit new_tx_freq is written by the I2C interrupt. Reading it
	// here can catch it half written, which is why nothing is sent until
	// the value has held still for BAND_SETTLE_MS.
	uint64_t f = new_tx_freq;
	if (f != tx_freq) {
		tx_freq = f;
		freq_set_at = now_ms();
	}

	if (Registers[REG_JUMA_MODE] & JUMA_MODE_NO_BAND) {
		want_band = 0;
		return;
	}
	// Nothing known yet. Do not guess a band: the wrong filter is what
	// destroys an output stage, and a PA left where the operator put it is
	// always the safer state.
	if (tx_freq == 0 || tx_freq > 0xFFFFFFFFull) {
		want_band = 0;
		return;
	}

	uint8_t target = bandFromHz((uint32_t)tx_freq);
	want_band = target;
	// A band the PA has no filter for - 6 m, 60 m, and out-of-band. No '=Bn'
	// may go out for these.
	if (target == BAND_NONE)
		return;
	if (now_ms() - freq_set_at < BAND_SETTLE_MS)
		return;
	if (transmitting())
		return;
	if (!online())
		return;

	if (st.band == target) {
		// Right band - but while we do the selecting, the PA must not also
		// be selecting: its own F-Sense would eventually pull it elsewhere.
		// A '=Bn' for the band already in use moves it from A to M and
		// changes nothing else.
		if (st.autoSel) {
			if (force_m_tries < RETRY_MAX && now_ms() - last_force_m >= RETRY_GAP_MS) {
				if (set_band(target)) {
					last_force_m = now_ms();
					force_m_tries++;
				}
			}
		} else {
			force_m_tries = 0;
		}
		return;
	}

	if (now_ms() - last_band_cmd < BAND_REPEAT_MS)
		return;
	if (set_band(target)) {
		last_band_cmd = now_ms();
		// A band change can knock the PA out of OPERATE, so the STANDBY that
		// may follow is expected and the hold below gets its tries back.
		operate_tries = 0;
	}
}

// --- OPERATE, held on request --------------------------------------------
static void hold_operate(void)
{
	if (!(Registers[REG_JUMA_MODE] & JUMA_MODE_HOLD_OPERATE)) {
		operate_tries = 0;
		return;
	}
	if (!online() || transmitting())
		return;
	// Never argue with a protective shutdown. An alarm is cleared with
	// JUMA_CMD_CLR_ALARM, by someone who has looked at why it tripped.
	if (st.alarms)
		return;
	if (st.operate) {
		operate_tries = 0;
		return;
	}
	if (operate_tries >= RETRY_MAX || now_ms() - last_operate < RETRY_GAP_MS)
		return;
	if (enqueue("=O")) {
		last_operate = now_ms();
		operate_tries++;
	}
}

// --- The command register -------------------------------------------------
static void run_command(void)
{
	uint8_t cmd = Registers[REG_JUMA_CMD];
	if (cmd == 0)
		return;

	const char *out = NULL;
	switch (cmd) {
	case JUMA_CMD_OPERATE:    out = "=O";  break;
	case JUMA_CMD_STANDBY:    out = "=S";  break;
	case JUMA_CMD_AUTO_BAND:  out = "=A";  break;
	case JUMA_CMD_CLR_ALARM:  out = "=C";  break;
	case JUMA_CMD_POWER_OFF:  out = "=P0"; break;
	case JUMA_CMD_POWER_SAVE: out = "=P1"; break;
	default:
		Registers[REG_FAULT] |= JUMA_FAULT_BAD_CMD;
		Registers[REG_JUMA_CMD] = 0;
		return;
	}
	// Leave the register standing if the queue is full - then the host can
	// still see its command has not gone out, and the next pass sends it.
	if (enqueue(out)) {
		// '=S' and an OPERATE hold would fight each other; an explicit
		// STANDBY wins and switches the hold off.
		if (cmd == JUMA_CMD_STANDBY)
			Registers[REG_JUMA_MODE] &= (uint8_t)~JUMA_MODE_HOLD_OPERATE;
		// After '=A' the PA does its own band selection, and ours would
		// immediately take it back.
		if (cmd == JUMA_CMD_AUTO_BAND)
			Registers[REG_JUMA_MODE] |= JUMA_MODE_NO_BAND;
		Registers[REG_JUMA_CMD] = 0;
	}
}

// The attenuator and a band by hand. Separate registers rather than more codes
// in REG_JUMA_CMD, because both carry a value: write it, and the firmware clears
// the register once the command has gone out.
static void run_setters(void)
{
	uint8_t g = Registers[REG_JUMA_SET_GAIN];
	if (g) {
		if (g > 4) {
			Registers[REG_FAULT] |= JUMA_FAULT_BAD_CMD;
			Registers[REG_JUMA_SET_GAIN] = 0;
		} else if (set_gain(g)) {
			Registers[REG_JUMA_SET_GAIN] = 0;
		}
	}

	uint8_t b = Registers[REG_JUMA_SET_BAND];
	if (b) {
		// Only while band following is off. Otherwise the follower would put
		// the PA back on the next frequency change, and a filter that moves
		// on its own is the last thing anybody wants.
		if (b > 9 || !(Registers[REG_JUMA_MODE] & JUMA_MODE_NO_BAND)) {
			Registers[REG_FAULT] |= JUMA_FAULT_BAD_CMD;
			Registers[REG_JUMA_SET_BAND] = 0;
		} else if (!transmitting() && set_band(b)) {
			Registers[REG_JUMA_SET_BAND] = 0;
		}
	}
}

// Watch REG_JUMA_MODE and put it in flash once it has settled.
static void save_mode_if_settled(void)
{
	uint8_t mode = Registers[REG_JUMA_MODE];

	if (mode != mode_last) {
		mode_last = mode;
		mode_changed_at = now_ms();
		return;
	}
	if (!mode_changed_at)                       // nothing has changed since boot
		return;
	if (now_ms() - mode_changed_at < SAVE_SETTLE_MS)
		return;
	if (saved_valid && saved_mode == mode) {    // already in flash
		mode_changed_at = 0;
		return;
	}
	// Not in the middle of a transmission: the erase stops the receive
	// interrupt for tens of milliseconds, and that is not the moment.
	if (transmitting())
		return;

	save_store(mode);
	mode_changed_at = 0;
#if JUMA_DEBUG
	printf("mode %02X saved to flash\n", mode);
#endif
}

// Defined with publish() below, which is where the rest of the scaling lives.
static uint16_t scale(float v, float f);

// --- The checkable snapshot ------------------------------------------------
// Payloads first, tags afterwards. The I2C interrupt can serve a read in the
// middle of this, and then it sees a group whose tag is still the old one -
// which is exactly the mismatch the host is watching for. The other way round,
// tags first, would hand out a group that looks coherent and is not.
static uint8_t snap_gen = 0;

static void take_snapshot(void)
{
	if (!Registers[REG_JUMA_SNAP])
		return;
	Registers[REG_JUMA_SNAP] = 0;

	uint16_t v = scale(st.volts, 100.0f);
	uint16_t a = scale(st.amps, 100.0f);
	uint16_t w = scale(st.watts, 10.0f);
	uint16_t r = scale(st.swr, 100.0f);

	Registers[REG_SNAP_A + 1] = online() ? 1 : 0;
	Registers[REG_SNAP_A + 2] = Registers[REG_JUMA_FLAGS];
	Registers[REG_SNAP_A + 3] = st.band;

	Registers[REG_SNAP_B + 1] = want_band;
	Registers[REG_SNAP_B + 2] = st.gain;
	Registers[REG_SNAP_B + 3] = (uint8_t)(st.alarms & 0xFF);

	Registers[REG_SNAP_C + 1] = (uint8_t)(v >> 8);
	Registers[REG_SNAP_C + 2] = (uint8_t)(v & 0xFF);
	Registers[REG_SNAP_C + 3] = Registers[REG_JUMA_TEMP];

	Registers[REG_SNAP_D + 1] = (uint8_t)(a >> 8);
	Registers[REG_SNAP_D + 2] = (uint8_t)(a & 0xFF);
	Registers[REG_SNAP_D + 3] = st.fan;

	Registers[REG_SNAP_E + 1] = (uint8_t)(w >> 8);
	Registers[REG_SNAP_E + 2] = (uint8_t)(w & 0xFF);
	Registers[REG_SNAP_E + 3] = Registers[REG_JUMA_MODE];

	Registers[REG_SNAP_F + 1] = (uint8_t)(r >> 8);
	Registers[REG_SNAP_F + 2] = (uint8_t)(r & 0xFF);
	Registers[REG_SNAP_F + 3] = Registers[REG_FAULT];

	Registers[REG_SNAP_G + 1] = (uint8_t)(replies & 0xFF);
	Registers[REG_SNAP_G + 2] = (uint8_t)(bad_lines & 0xFF);
	Registers[REG_SNAP_G + 3] = (uint8_t)(rx_lost & 0xFF);

	// Generation 1..31 in the top five bits, the group's own number in the
	// bottom three. A group from an older snapshot fails on the generation; a
	// group standing in for another one fails on the number.
	if (++snap_gen > 31)
		snap_gen = 1;
	for (uint8_t g = 0; g < REG_SNAP_GROUPS; g++)
		Registers[REG_SNAP_A + 4 * g] = (uint8_t)((snap_gen << 3) | g);
}

// --- Telemetry on the USB port -------------------------------------------
// One line per second, space separated key=value, so a program on the PC can
// read it with a split(). Everything after 'raw=' is the PA's own status line
// with the blanks taken out - which means it can be fed straight back into
// jumaParseStatus(), the same parser both firmwares here use. No value is
// rescaled on the way, so nothing is lost to rounding.
//
// Compiled in unconditionally: JUMA_MODE_TELEMETRY can be set over I2C, so a
// host tool can switch the feed on without the board being reflashed.
static const uint32_t TELEMETRY_MS = 1000;
static uint32_t last_tel = 0;

static void telemetry(void)
{
	if (!(Registers[REG_JUMA_MODE] & JUMA_MODE_TELEMETRY))
		return;
	// A 'JUMA ...' line in a proxy stream would be something the PA never
	// says. The proxy carries the same information anyway.
	if (proxy_on())
		return;
	if (now_ms() - last_tel < TELEMETRY_MS)
		return;
	last_tel = now_ms();

	char raw[sizeof(st.raw)];
	size_t o = 0;
	for (const char *c = st.raw; *c && o < sizeof(raw) - 1; c++)
		if (*c != ' ')
			raw[o++] = *c;
	raw[o] = '\0';

	printf("JUMA up=%lu link=%u mode=%02X fault=%02X want=%u "
	       "rep=%lu bad=%lu lost=%u raw=%s\n",
	       (unsigned long)(now_ms() / 1000), online() ? 1u : 0u,
	       Registers[REG_JUMA_MODE], Registers[REG_FAULT], want_band,
	       (unsigned long)replies, (unsigned long)bad_lines,
	       (unsigned)rx_lost, raw);
}

// --- Status into the registers -------------------------------------------
// 16 bit, most significant byte first - the order the rest of the board uses
// for the transmit frequency.
static void put16(uint8_t reg, uint16_t v)
{
	Registers[reg]     = (uint8_t)(v >> 8);
	Registers[reg + 1] = (uint8_t)(v & 0xFF);
}

static uint16_t scale(float v, float f)
{
	if (v <= 0.0f)
		return 0;
	float x = v * f + 0.5f;
	return (x > 65535.0f) ? 65535 : (uint16_t)x;
}

static uint8_t scale10(float v)
{
	if (v <= 0.0f)
		return 0;
	float x = v * 10.0f + 0.5f;
	return (x > 255.0f) ? 255 : (uint8_t)x;
}

static void publish(void)
{
	const bool up = online();

	Registers[REG_JUMA_LINK] = up ? 1 : 0;

	uint8_t fl = 0;
	if (st.operate)  fl |= JUMA_FLAG_OPERATE;
	if (st.autoSel)  fl |= JUMA_FLAG_AUTO_SEL;
	if (st.tx)       fl |= JUMA_FLAG_PA_TX;
	if (st.celsius)  fl |= JUMA_FLAG_CELSIUS;
	if (!(Registers[REG_JUMA_MODE] & JUMA_MODE_NO_BAND))
		fl |= JUMA_FLAG_FOLLOWING;
	if (!gpio_get(GPIO13_EXTTR))
		fl |= JUMA_FLAG_HL2_TX;
	Registers[REG_JUMA_FLAGS] = fl;

	// The measurements are the last ones received, whether or not the link is
	// still alive. REG_JUMA_LINK is what says how much to trust them, and
	// REG_JUMA_REPLIES counting up is the other way to tell.
	Registers[REG_JUMA_BAND]      = st.band;
	Registers[REG_JUMA_WANT_BAND] = want_band;
	Registers[REG_JUMA_GAIN]      = st.gain;
	Registers[REG_JUMA_ALARMS]    = (uint8_t)(st.alarms & 0xFF);
	Registers[REG_JUMA_SWR]       = scale10(st.swr);
	Registers[REG_JUMA_VOLTS]     = scale10(st.volts);
	Registers[REG_JUMA_AMPS]      = scale10(st.amps);

	int t = st.temp;
	if (t > 127)  t = 127;
	if (t < -128) t = -128;
	Registers[REG_JUMA_TEMP] = (uint8_t)(int8_t)t;

	int w = (int)(st.watts + 0.5f);
	if (w < 0)     w = 0;
	if (w > 65535) w = 65535;
	Registers[REG_JUMA_WATTS_MSB] = (uint8_t)(w >> 8);
	Registers[REG_JUMA_WATTS_LSB] = (uint8_t)(w & 0xFF);

	Registers[REG_JUMA_FAN]      = st.fan;
	Registers[REG_JUMA_REPLIES]  = (uint8_t)(replies & 0xFF);
	Registers[REG_JUMA_BADLINES] = (uint8_t)(bad_lines & 0xFF);
	Registers[REG_JUMA_LOST]     = (uint8_t)(rx_lost & 0xFF);

	// The same measurements at the PA's own precision, for a display that
	// wants 13.66 V rather than 13.6.
	put16(REG_JUMA_VOLTS100_MSB, scale(st.volts, 100.0f));
	put16(REG_JUMA_AMPS100_MSB,  scale(st.amps,  100.0f));
	put16(REG_JUMA_WATTS10_MSB,  scale(st.watts,  10.0f));
	put16(REG_JUMA_SWR100_MSB,   scale(st.swr,   100.0f));

	// A window onto the banner: the host writes the index, reads the
	// character. Index 0 answers with the length, so it knows how far to go.
	uint8_t bi = Registers[REG_JUMA_BANNER_IDX];
	Registers[REG_JUMA_BANNER_CH] =
		(bi == 0) ? banner_len :
		(bi <= banner_len) ? (uint8_t)banner[bi - 1] : 0;

	Registers[REG_JUMA_SAVED] = saved_valid ? 1 : 0;

	// REG_FAULT is a bit field here. JUMA_FAULT_BAD_CMD is set by
	// run_command() and stays until the host resets the registers.
	uint8_t fault = Registers[REG_FAULT] & JUMA_FAULT_BAD_CMD;
	if (!up)
		fault |= JUMA_FAULT_NO_LINK;
	if (up && st.alarms)
		fault |= JUMA_FAULT_ALARM;
	Registers[REG_FAULT] = fault;
}

// A command from the USB side, whether typed by a person or sent by a program.
// Almost everything goes straight through; the two that touch band selection
// cannot, because this firmware is doing the selecting:
//
//   '=Bn'  only with band following switched off - otherwise the follower would
//          move the filter back on the next frequency change, and a filter that
//          moves on its own is the last thing anybody wants.
//   '=A'   hands selection to the PA, so it switches following off as it goes,
//          exactly as JUMA_CMD_AUTO_BAND does.
//
// Returns false when it was refused, and says why unless the port is a proxy -
// there a stray line would corrupt a strict client's stream.
static bool submit_from_usb(const char *cmd)
{
	const bool following = !(Registers[REG_JUMA_MODE] & JUMA_MODE_NO_BAND);

	if (cmd[0] == '=' && cmd[1] == 'B' && following) {
		if (!proxy_on())
			printf("refused: %s - band following is on, "
			       "set REG_JUMA_MODE bit 0 first\n", cmd);
		return false;
	}
	if (!enqueue(cmd))
		return false;
	if (cmd[0] == '=' && cmd[1] == 'A')
		Registers[REG_JUMA_MODE] |= JUMA_MODE_NO_BAND;
	return true;
}

// Type a command such as "=R" or "=C" on the USB serial port and it goes to the
// PA. This is in every image, not just the talking one: on a board with no
// software that knows the I2C registers it is the only way to clear an alarm by
// hand. The ESP32 firmware has a telnet console for the same reason.
//
// Polled every CONSOLE_POLL_MS rather than on every pass - each call takes the
// stdio mutex, and a thousand times a second for something a human types is
// wasteful.
static const uint32_t CONSOLE_POLL_MS = 20;
static uint32_t last_console = 0;

// One or two hex digits. Returns the character after them, or NULL if there was
// no digit - hand rolled rather than sscanf, which would pull the whole scanf
// machinery into the image for this.
static const char *hex8(const char *p, uint8_t *out)
{
	uint16_t v = 0;
	int n = 0;
	for (; n < 2; n++, p++) {
		char c = *p;
		if      (c >= '0' && c <= '9') v = (uint16_t)(v * 16 + (c - '0'));
		else if (c >= 'a' && c <= 'f') v = (uint16_t)(v * 16 + (c - 'a' + 10));
		else if (c >= 'A' && c <= 'F') v = (uint16_t)(v * 16 + (c - 'A' + 10));
		else break;
	}
	if (!n)
		return NULL;
	*out = (uint8_t)v;
	return p;
}

// 'mode=HH' or 'reg=HH:HH'. Returns true when the line was one of them, so the
// caller knows not to pass it on to the amplifier.
static bool console_register(const char *in)
{
	uint8_t reg, val;
	const char *p;

	// 'mode=HH' replaces the byte, 'mode+HH' sets those bits and 'mode-HH'
	// clears them. The last two matter: a program that wants the telemetry
	// feed must be able to switch it on without also deciding, blindly, what
	// should happen to the OPERATE hold.
	if (!strncmp(in, "mode", 4) &&
	    (in[4] == '=' || in[4] == '+' || in[4] == '-')) {
		if (hex8(in + 5, &val)) {
			if      (in[4] == '+') Registers[REG_JUMA_MODE] |= val;
			else if (in[4] == '-') Registers[REG_JUMA_MODE] &= (uint8_t)~val;
			else                   Registers[REG_JUMA_MODE]  = val;
		}
		if (!proxy_on())
			printf("mode=%02X\n", (unsigned)Registers[REG_JUMA_MODE]);
		return true;
	}
	if (!strncmp(in, "reg=", 4)) {
		p = hex8(in + 4, &reg);
		if (p && *p == ':' && hex8(p + 1, &val)) {
			Registers[reg] = val;
			if (!proxy_on())
				printf("reg=%02X:%02X\n", reg, val);
		}
		return true;
	}
	return false;
}

// Long enough for 'reg=HH:HH' and a stray character or two - the command queue's
// QL only has to hold what the PA itself accepts, which is at most '=P0'.
#define CONSOLE_LINE 16

static void usb_console(void)
{
	static char in[CONSOLE_LINE] = "";
	static uint8_t n = 0;

	if (now_ms() - last_console < CONSOLE_POLL_MS)
		return;
	last_console = now_ms();

	int ch = getchar_timeout_us(0);
	while (ch != PICO_ERROR_TIMEOUT) {
		if (ch == '\r' || ch == '\n') {
			if (n) {
				in[n] = '\0';
				// Two lines that are not for the PA. They give the USB
				// side the same reach into the registers that a host on
				// the I2C bus has, so a program on the PC can work over
				// one cable and never touch the HL2:
				//     mode=06        set REG_JUMA_MODE, hex
				//     reg=51:02      write any register, hex:hex
				// Recognised in proxy mode as well - the PA has no
				// command that looks remotely like this.
				if (console_register(in)) {
					n = 0;
					ch = getchar_timeout_us(0);
					continue;
				}
				// Not a 'JUMA ' line, so a consumer of the telemetry
				// feed ignores it - but a person sees the command took.
				// A proxy stream stays clean: nothing but PA traffic.
				bool ok = submit_from_usb(in);
				if (!proxy_on() && !ok && strlen(in) >= QL)
					printf("too long: %s\n", in);
				else if (!proxy_on())
					printf("%s: %s\n", ok ? "sent" : "not sent", in);
				n = 0;
			}
		} else if (n < sizeof(in) - 1) {
			in[n++] = (char)ch;
		}
		ch = getchar_timeout_us(0);
	}
}

int main(void)
{
	uint32_t last_poll = 0;

	stdio_init_all();
	configure_pins(true, false);	// J4 pin 1 and J8 pin 1 become the UART
	configure_led_flasher();

	// Switched 5 V on, so a level shifter fed from Sw5 comes up with the
	// board. configure_pins() leaves it off, and a MAX3232 without power
	// looks exactly like a dead amplifier. If the shifter is wired to the
	// unswitched 5V1 pad instead this changes nothing.
	gpio_put(GPIO12_Sw5, 1);

	uart_init(UART_ID, BAUD_RATE);
	uart_set_hw_flow(UART_ID, false, false);
	uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);
	uart_set_fifo_enabled(UART_ID, true);

	// A mode kept from last time wins over the one built in: it is what the
	// operator chose, and the built-in value is only the state of a board that
	// has never been told anything.
	uint8_t boot_mode = JUMA_MODE_AT_BOOT;
	if (save_load(&saved_mode)) {
		saved_valid = true;
		boot_mode = saved_mode;
	}
	Registers[REG_JUMA_MODE] = boot_mode;
	mode_last = boot_mode;

	// Let the SDR software recognise the board, or it will not send the
	// transmit frequency at all and nothing downstream of it can work.
	Registers[REG_LPF_DETECT] = LPF_DETECT_MAGIC;
	Registers[REG_LPF_STATUS] = 0;
	IrqHandler[REG_CONTROL]  = on_control_written;

	int UART_IRQ = UART_ID == uart0 ? UART0_IRQ : UART1_IRQ;
	irq_set_exclusive_handler(UART_IRQ, on_uart_rx);
	irq_set_enabled(UART_IRQ, true);
	uart_set_irq_enables(UART_ID, true, false);	// receive only

	while (1) {
		sleep_ms(1);			// this sets the polling frequency

		if (host_reset) {
			host_reset = false;
			// The library has just zeroed every register, new_tx_freq
			// included. Put our default back and start over, so the
			// retry counters do not carry a state that no longer exists.
			// 'Power-up condition' now means what is in flash, if
			// anything is - the same thing a real power cycle gives.
			Registers[REG_LPF_DETECT] = LPF_DETECT_MAGIC;
			Registers[REG_JUMA_MODE] =
				saved_valid ? saved_mode : (uint8_t)JUMA_MODE_AT_BOOT;
			mode_last = Registers[REG_JUMA_MODE];
			mode_changed_at = 0;
			tx_freq = 0;
			want_band = 0;
			force_m_tries = 0;
			operate_tries = 0;
#if JUMA_DEBUG
			printf("host reset - mode back to 0x%02X\n", JUMA_MODE_AT_BOOT);
#endif
		}

		drain_rx();
		run_command();
		run_setters();
		band_control();
		hold_operate();

		// The poll goes last so a command that came up in this pass is
		// ahead of it in the queue - and it is this poll that keeps the PA
		// in remote mode at all.
		if (now_ms() - last_poll >= POLL_INTERVAL_MS) {
			last_poll = now_ms();
			enqueue("=R");
		}

		pump_queue();
		publish();
		take_snapshot();		// after publish(), which fills what it copies
		save_mode_if_settled();
		telemetry();
		usb_console();
	}
}
