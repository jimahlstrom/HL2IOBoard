#pragma once
#include <stdint.h>

// JUMA band index as used by '=Bn' and status field 5.
// 1 = 160 m ... 9 = 10 m, 10 = "Unknown Band" (only as a status reply).
enum : uint8_t { BAND_NONE = 0, BAND_UNKNOWN = 10 };

// Frequency -> JUMA band index. 0 when the PA does not cover the band
// (6 m, 4 m, 2 m, 60 m, LF/MF) or the frequency is in no band at all.
uint8_t  bandFromHz(uint32_t hz);

// Plain name for display and logs. With bandFromHz()==0 this returns the name
// of the recognised but unsupported band, otherwise "-".
const char* bandName(uint8_t jumaIndex);
const char* bandNameFromHz(uint32_t hz);
