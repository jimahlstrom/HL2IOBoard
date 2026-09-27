#!/bin/sh
# Host tests for the status parser and band mapping - no Pico needed.
#
# juma_status.* and bands.* are copies. The originals are src/ in the ESP32
# firmware for the same amplifier, because both controllers have to agree about
# what band it is on - a band edge corrected in one place and not the other and
# they disagree on the air. tools/sync_shared.py says when the copy is stale;
# these say when it is wrong.
set -e
cd "$(dirname "$0")/.."
c++ -std=c++17 -Wall -I . tests/test_parse.cpp juma_status.cpp bands.cpp -o "${TMPDIR:-/tmp}/juma-tests"
exec "${TMPDIR:-/tmp}/juma-tests"
