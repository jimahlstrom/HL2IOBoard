// Host tests for the status parser and the band mapping.
//   clang++ -std=c++17 -I src tests/test_parse.cpp src/juma_status.cpp src/bands.cpp -o /tmp/t && /tmp/t
#include "juma_status.h"
#include "bands.h"
#include <cstdio>
#include <cstring>
#include <cmath>

static int fails = 0, checks = 0;

#define CHECK(cond, ...) do { checks++; if (!(cond)) { \
    fails++; printf("  FAIL %s:%d  ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static bool feq(float a, float b) { return std::fabs(a - b) < 0.005f; }

// The parser works in place - so always on a copy.
static bool parse(const char* s, JumaStatus& st) {
    char buf[160];
    std::strncpy(buf, s, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    return jumaParseStatus(buf, st);
}

static void test_manual_example() {
    printf("Example from the manual (annex D)\n");
    JumaStatus st;
    // O:A:T:C: 5:1:1.0:14.09: 8.1: 27.2: 26:0: 0
    CHECK(parse("O:A:T:C: 5:1:1.0:14.09: 8.1: 27.2: 26:0: 0", st), "should parse");
    CHECK(st.valid,            "valid");
    CHECK(st.operate,          "field 1 O -> operate");
    CHECK(st.autoSel,          "field 2 A -> autoSel");
    CHECK(st.tx,               "field 3 T -> tx");
    CHECK(st.celsius,          "field 4 C -> celsius");
    CHECK(st.band == 5,        "field 5 band = 5, got %u", st.band);
    CHECK(st.gain == 1,        "field 6 gain = 1, got %u", st.gain);
    CHECK(feq(st.swr, 1.0f),   "field 7 SWR = 1.0, got %.2f", st.swr);
    CHECK(feq(st.volts, 14.09f), "field 8 volts = 14.09, got %.2f", st.volts);
    CHECK(feq(st.amps, 8.1f),  "field 9 amps = 8.1, got %.2f", st.amps);
    CHECK(feq(st.watts, 27.2f),"field 10 watts = 27.2, got %.2f", st.watts);
    CHECK(st.temp == 26,       "field 11 temp = 26, got %d", st.temp);
    CHECK(st.fan == 0,         "field 12 fan = 0, got %u", st.fan);
    CHECK(st.alarms == 0,      "field 13 alarm = 0, got 0x%02X", st.alarms);
}

static void test_standby_variant() {
    printf("STANDBY / Manual / RX / Fahrenheit\n");
    JumaStatus st;
    CHECK(parse("S:M:R:F: 3:4:1.4:13.80: 0.4:  0.0: 79:2: 0", st), "should parse");
    CHECK(!st.operate, "S -> standby");
    CHECK(!st.autoSel, "M -> manual");
    CHECK(!st.tx,      "R -> rx");
    CHECK(!st.celsius, "F -> Fahrenheit");
    CHECK(st.band == 3, "band 3 (40m)");
    CHECK(st.gain == 4, "gain 4");
    CHECK(feq(st.watts, 0.0f), "0 W");
    CHECK(st.temp == 79, "temp 79");
    CHECK(st.fan == 2, "fan medium");
}

static void test_alarms_are_hex() {
    printf("Alarm field is hexadecimal, not decimal\n");
    JumaStatus st;

    // "10" hex = Bit 4 = Low Voltage Pre-Limit.
    // Read as decimal it would be 10 = 0b1010 = over-current + high voltage.
    CHECK(parse("O:A:R:C: 5:1:1.0:11.20: 0.2:  0.0: 30:0: 10", st), "parses");
    CHECK(st.alarms == ALARM_LOW_VOLT_PRE, "0x10 -> LOW_VOLT_PRE only, got 0x%02X", st.alarms);
    CHECK(!(st.alarms & ALARM_OVERCURRENT), "no spurious over-current");
    CHECK(!(st.alarms & ALARM_HIGH_VOLT),   "no spurious high voltage");

    // "20" hex = bit 5. Decimal would be 32 = bit 5 - coincidentally equal, but
    // "30" separates the cases: hex = bits 4|5, decimal 30 = 0b11110.
    CHECK(parse("O:A:R:C: 5:1:1.0:10.50: 0.2:  0.0: 30:0: 30", st), "parses");
    CHECK(st.alarms == (ALARM_LOW_VOLT_PRE | ALARM_LOW_VOLT_FINAL),
          "0x30 -> both low-voltage bits, got 0x%02X", st.alarms);
    CHECK(!(st.alarms & ALARM_HIGH_TEMP), "no spurious high temperature");

    CHECK(parse("O:A:T:C: 9:2:3.1:12.00:12.0:110.0: 75:3: 3F", st), "parses");
    CHECK(st.alarms == 0x3F, "0x3F -> all six bits, got 0x%02X", st.alarms);

    CHECK(parse("O:A:T:C: 9:2:3.1:12.00:12.0:110.0: 75:3: 1", st), "parses");
    CHECK(st.alarms == ALARM_HIGH_SWR, "0x01 -> High SWR");

    CHECK(parse("O:A:T:C: 9:2:1.1:13.80: 9.0: 99.0: 60:3: c", st), "parses");
    CHECK(st.alarms == 0x0C, "lower-case hex 'c' -> 0x0C, got 0x%02X", st.alarms);
}

static void test_edge_cases() {
    printf("Edge cases\n");
    JumaStatus st;

    CHECK(parse("O:A:R:C:10:1:1.0:13.80: 0.3:  0.0: 25:0: 0", st), "parses");
    CHECK(st.band == BAND_UNKNOWN, "band 10 = unknown band, got %u", st.band);

    CHECK(!parse("O:A:T:C: 5:1:1.0", st), "too few fields -> rejected");
    CHECK(!parse("X:A:T:C: 5:1:1.0:14.09: 8.1: 27.2: 26:0: 0", st),
          "field 1 neither O nor S -> rejected");
    CHECK(!parse("", st), "empty line -> rejected");
    CHECK(!parse("Juma PA-100D V4.00a", st), "boot message -> rejected");
    // Nothing in the parser knows a model name - other units announce
    // themselves differently, and all of them must fail the same way.
    CHECK(!parse("RS-928 V1.05q", st),        "clone banner -> rejected");
    CHECK(!parse("Juma PA1000 V1.65", st),    "PA1000 banner -> rejected");
    CHECK(!parse("", st),                     "empty line -> rejected");

    // Firmware that appends further fields must not block everything.
    CHECK(parse("O:A:T:C: 5:1:1.0:14.09: 8.1: 27.2: 26:0: 0:XX", st),
          "14 fields -> still parses");
    CHECK(st.band == 5, "band field stays correct");

    // raw must reproduce the line (the parser cuts it up in place)
    CHECK(parse("O:A:T:C: 5:1:1.0:14.09: 8.1: 27.2: 26:0: 0", st), "parses");
    CHECK(!std::strcmp(st.raw, "O:A:T:C: 5:1:1.0:14.09: 8.1: 27.2: 26:0: 0"),
          "raw reconstructed, got '%s'", st.raw);

    // Three-digit temperature and 100+ W
    CHECK(parse("O:A:T:C: 9:4:1.2:13.60:18.5:105.4:103:3: 4", st), "parses");
    CHECK(st.temp == 103, "temp 103, got %d", st.temp);
    CHECK(feq(st.watts, 105.4f), "105.4 W, got %.1f", st.watts);
    CHECK(st.alarms == ALARM_HIGH_TEMP, "High Temperature");
}

struct BandCase { uint32_t hz; uint8_t juma; const char* name; };

static void test_bands() {
    printf("Frequency -> band index\n");
    static const BandCase cases[] = {
        { 1840000, 1, "160m"}, { 1999000, 1, "160m"},
        { 3573000, 2, "80m" }, { 3800000, 2, "80m" },
        { 7074000, 3, "40m" }, { 7200000, 3, "40m" },
        {10136000, 4, "30m" },
        {14074000, 5, "20m" }, {14350000, 5, "20m" },
        {18100000, 6, "17m" },
        {21074000, 7, "15m" },
        {24915000, 8, "12m" },
        {28074000, 9, "10m" }, {29600000, 9, "10m" },
        // recognised but not covered by the PA-100D -> 0, no =Bn
        {50313000, 0, "6m"  }, { 5357000, 0, "60m" },
        {70200000, 0, "4m"  }, {144300000,0, "2m"  },
        {  474200, 0, "630m"}, {  137500, 0, "2200m"},
        // no band at all
        { 9000000, 0, "-"   }, {15000000, 0, "-"   }, {       0, 0, "-" },
    };
    for (const auto& c : cases) {
        uint8_t got = bandFromHz(c.hz);
        CHECK(got == c.juma, "%u Hz -> expected %u (%s), got %u",
              c.hz, c.juma, c.name, got);
        CHECK(!std::strcmp(bandNameFromHz(c.hz), c.name),
              "%u Hz -> name '%s', got '%s'", c.hz, c.name, bandNameFromHz(c.hz));
    }
    // bandName() for the status display
    CHECK(!std::strcmp(bandName(1), "160m"), "bandName(1)");
    CHECK(!std::strcmp(bandName(9), "10m"),  "bandName(9)");
    CHECK(!std::strcmp(bandName(10), "?"),   "bandName(10) = unknown");
    CHECK(!std::strcmp(bandName(0), "-"),    "bandName(0)");
}

// The Pico firmware for the HL2 IO board (hl2io/juma_pa) puts the PA's status
// line on its USB port as the 'raw=' field of a telemetry line, with the blanks
// taken out so the whole line stays space separated key=value. That only holds
// up if a blank-stripped line parses to exactly the same thing - otherwise a
// consumer on the PC quietly reads different values than the board did.
static void test_blanks_are_not_information() {
    printf("A status line without its blanks parses the same\n");
    const char* lines[] = {
        "O:A:T:C: 5:1:1.0:14.09: 8.1: 27.2: 26:0: 0",
        "S:M:R:F:10:4:2.9:13.10:10.4: 96.0:147:3: A",
        "O:M:R:C: 3:2:1.4:13.66: 0.0:  0.0: 24:1: 0",
    };
    for (const char* src : lines) {
        char stripped[160];
        size_t o = 0;
        for (const char* c = src; *c && o < sizeof(stripped) - 1; c++)
            if (*c != ' ') stripped[o++] = *c;
        stripped[o] = '\0';

        JumaStatus a, b;
        CHECK(parse(src, a), "'%s' should parse", src);
        CHECK(parse(stripped, b), "'%s' should parse", stripped);
        CHECK(a.operate == b.operate && a.autoSel == b.autoSel &&
              a.tx == b.tx && a.celsius == b.celsius,
              "flags differ for '%s'", stripped);
        CHECK(a.band == b.band && a.gain == b.gain && a.fan == b.fan,
              "band/gain/fan differ for '%s'", stripped);
        CHECK(feq(a.swr, b.swr) && feq(a.volts, b.volts) &&
              feq(a.amps, b.amps) && feq(a.watts, b.watts),
              "measurements differ for '%s'", stripped);
        CHECK(a.temp == b.temp, "temp %d vs %d for '%s'", a.temp, b.temp, stripped);
        CHECK(a.alarms == b.alarms, "alarms %02X vs %02X for '%s'",
              a.alarms, b.alarms, stripped);
    }
}

int main() {
    test_manual_example();
    test_standby_variant();
    test_alarms_are_hex();
    test_edge_cases();
    test_bands();
    test_blanks_are_not_information();
    printf("\n%d checks, %d failures\n", checks, fails);
    return fails ? 1 : 0;
}
