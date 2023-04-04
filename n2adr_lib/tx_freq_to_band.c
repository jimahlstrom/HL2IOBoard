// This is firmware for the Hermes Lite 2 IO board designed by Jim Ahlstrom, N2ADR. It is
//   Copyright (c) 2022-2023 James C. Ahlstrom <jahlstr@gmail.com>.
//   It is licensed under the MIT license. See MIT.txt.

// Return the band index given the frequency.

#include "../hl2ioboard.h"

#define MAX_FREQ_INDEX	23
int tx_freq_to_band(uint64_t freq)
{
// Band  137k  500k  160  80  60  40  30  20  17  15  12  10   6   4   2  1.25  70cm  33cm  23cm  13cm  9cm  5cm  3cm
// Index    1     2    3   4   5   6   7   8   9  10  11  12  13  14  15    16    17    18    19    20   21   22   23
	static uint8_t index = 8;
// Table of cutoff frequencies. The correct index for the band is at: cutoff[index - 1] <= freq <= cutoff[index].
	static uint64_t cutoff[MAX_FREQ_INDEX + 1] = {
          0,
     300000,	// 137k below this frequency
    1000000,	// 500k below this frequency
    2500000,	// 160m below this frequency
    4500000,	// 80m
    6000000,	// 60m
    8000000,	// 40m
   11000000,	// 30m
   16000000,	// 20m
   19000000,	// 17m
   22000000,	// 15m
   25000000,	// 12m
   32000000,	// 10m
   60000000,	// 6m
   90000000,	// 4m
  155000000,	// 2m
  300000000,	// 1.25
  500000000,	// 70cm
 1000000000,	// 33cm
 2000000000,	// 23cm
 3000000000,	// 13cm
 4000000000,	// 9cm
 7000000000,	// 5cm
11000000000,	// 3cm
	} ;

	if (cutoff[index - 1] <= freq && freq <= cutoff[index]) {
		//printf ("Keep index %d\n", index);
		return index;		// no change in band
	}
	for (index = 1; index < MAX_FREQ_INDEX; index++) {
		if (cutoff[index - 1] <= freq && freq <= cutoff[index])
			break;
	}
	//printf ("New index %d\n", index);
	return index;
}
