#include "juma_status.h"
#include <stdlib.h>
#include <string.h>

// First character of a right-justified field (skip leading blanks).
static char firstChar(const char* s) {
    while (*s == ' ' || *s == '\t') s++;
    return *s;
}

bool jumaParseStatus(char* line, JumaStatus& st) {
    // 13 fields separated by ':'. Newer firmware may append more.
    char* f[16];
    uint8_t n = 0;
    char* p = line;
    f[n++] = p;
    while (*p && n < 16) {
        if (*p == ':') { *p = '\0'; f[n++] = p + 1; }
        p++;
    }
    if (n < 13) return false;

    char op = firstChar(f[0]);
    if (op != 'O' && op != 'S') return false;

    st.operate = (op == 'O');
    st.autoSel = (firstChar(f[1]) == 'A');
    st.tx      = (firstChar(f[2]) == 'T');
    st.celsius = (firstChar(f[3]) == 'C');
    st.band    = (uint8_t)strtoul(f[4], nullptr, 10);
    st.gain    = (uint8_t)strtoul(f[5], nullptr, 10);
    st.swr     = (float)atof(f[6]);
    st.volts   = (float)atof(f[7]);
    st.amps    = (float)atof(f[8]);
    st.watts   = (float)atof(f[9]);
    st.temp    = (int)strtol(f[10], nullptr, 10);
    st.fan     = (uint8_t)strtoul(f[11], nullptr, 10);
    st.alarms  = (uint16_t)strtoul(f[12], nullptr, 16);   // HEX, see header

    // reassemble raw for the diagnostic display
    size_t o = 0;
    for (uint8_t i = 0; i < n && o < sizeof(st.raw) - 2; i++) {
        if (i) st.raw[o++] = ':';
        size_t l = strlen(f[i]);
        if (o + l >= sizeof(st.raw) - 1) l = sizeof(st.raw) - 1 - o;
        memcpy(st.raw + o, f[i], l);
        o += l;
    }
    st.raw[o] = '\0';

    st.valid = true;
    return true;
}
