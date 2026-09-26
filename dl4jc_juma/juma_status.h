#pragma once
#include <stdint.h>

// Free of Arduino dependencies so the parser can be tested on the host
// (see tests/test_parse.cpp).

// Alarm bits from status field 13. NOTE: the field is HEXADECIMAL
// (manual: "HH Hexadecimal bit-mapped alarm status"). Reading it as decimal
// decodes wrongly from 0x0A on: "10" is bit 4, not bits 1 and 3.
enum JumaAlarm : uint16_t {
    ALARM_HIGH_SWR       = 1 << 0,
    ALARM_OVERCURRENT    = 1 << 1,
    ALARM_HIGH_TEMP      = 1 << 2,
    ALARM_HIGH_VOLT      = 1 << 3,
    ALARM_LOW_VOLT_PRE   = 1 << 4,
    ALARM_LOW_VOLT_FINAL = 1 << 5,
};

struct JumaStatus {
    bool     valid    = false;   // at least one status reply understood
    uint32_t lastRxMs = 0;       // set by the driver, not by the parser
    bool     operate  = false;   // field 1: O / S
    bool     autoSel  = false;   // field 2: A / M  (the PA's own band select)
    bool     tx       = false;   // field 3: T / R
    bool     celsius  = true;    // field 4: C / F
    uint8_t  band     = 0;       // field 5: 1..9, 10 = unknown
    uint8_t  gain     = 0;       // field 6: 1..4  (really an attenuator)
    float    swr      = 0;       // field 7
    float    volts    = 0;       // field 8
    float    amps     = 0;       // field 9
    float    watts    = 0;       // field 10
    int      temp     = 0;       // field 11
    uint8_t  fan      = 0;       // field 12: 0=off 1=slow 2=medium 3=fast
    uint16_t alarms   = 0;       // field 13, hex
    char     raw[100] = {0};
};

// Splits a status reply from the PA, e.g.
//   O:A:T:C: 5:1:1.0:14.09: 8.1: 27.2: 26:0: 0
// The line is modified in place. Returns false when it is not a usable status
// reply (too few fields, field 1 neither O nor S).
bool jumaParseStatus(char* line, JumaStatus& st);
