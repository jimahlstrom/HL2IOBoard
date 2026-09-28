#!/bin/sh
# Host tests for the status parser and band mapping - no Pico needed.
#
# They cover juma_status.* and bands.*, which describe the amplifier rather than
# this board - the same table exists in the ESP32 firmware for the same PA, and a
# band edge corrected in only one of them leaves the two controllers disagreeing
# on the air. Nothing enforces that; these tests only say whether what is here
# behaves.
set -e
cd "$(dirname "$0")/.."
c++ -std=c++17 -Wall -I . tests/test_parse.cpp juma_status.cpp bands.cpp -o "${TMPDIR:-/tmp}/juma-tests"
exec "${TMPDIR:-/tmp}/juma-tests"
