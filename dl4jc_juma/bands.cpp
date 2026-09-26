#include "bands.h"
#include <stddef.h>

// Band edges widened slightly beyond the IARU R1 limits so a frequency right
// at a band edge is not missed. Order = search order.
struct BandRange {
    uint32_t lo, hi;
    uint8_t  juma;      // =Bn as the PA-100D numbers them, 0 = not covered
    const char* name;
};

static const BandRange RANGES[] = {
    // --- covered by the PA-100D (PA1000 numbering unverified) ---
    { 1790000,   2010000,   1, "160m" },
    { 3490000,   4010000,   2, "80m"  },
    { 6990000,   7310000,   3, "40m"  },
    {10090000,  10160000,   4, "30m"  },
    {13990000,  14360000,   5, "20m"  },
    {18060000,  18180000,   6, "17m"  },
    {20990000,  21460000,   7, "15m"  },
    {24880000,  25000000,   8, "12m"  },
    {27990000,  29710000,   9, "10m"  },
    // --- recognised but NOT covered: no =Bn may go out for these ---
    {  135700,      137800,   0, "2200m"},
    {  472000,      479000,   0, "630m" },
    { 5250000,   5450000,   0, "60m"  },
    {49990000,  54010000,   0, "6m"   },
    {69990000,  70510000,   0, "4m"   },
    {143990000,148010000,   0, "2m"   },
    {429990000,440010000,   0, "70cm" },
};
static const size_t N_RANGES = sizeof(RANGES) / sizeof(RANGES[0]);

static const BandRange* find(uint32_t hz) {
    for (size_t i = 0; i < N_RANGES; i++) {
        if (hz >= RANGES[i].lo && hz <= RANGES[i].hi) return &RANGES[i];
    }
    return nullptr;
}

uint8_t bandFromHz(uint32_t hz) {
    const BandRange* r = find(hz);
    return r ? r->juma : (uint8_t)BAND_NONE;
}

const char* bandNameFromHz(uint32_t hz) {
    const BandRange* r = find(hz);
    return r ? r->name : "-";
}

const char* bandName(uint8_t jumaIndex) {
    switch (jumaIndex) {
        case 1: return "160m";
        case 2: return "80m";
        case 3: return "40m";
        case 4: return "30m";
        case 5: return "20m";
        case 6: return "17m";
        case 7: return "15m";
        case 8: return "12m";
        case 9: return "10m";
        case BAND_UNKNOWN: return "?";
        default: return "-";
    }
}
