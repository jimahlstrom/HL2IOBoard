// This is firmware for the Hermes Lite 2 IO board designed by Jim Ahlstrom, N2ADR. It is
//   Copyright (c) 2022-2023 James C. Ahlstrom <jahlstr@gmail.com>.
//   It is licensed under the MIT license. See MIT.txt.

// Thanks to Clifford Heath for suggesting a logarithmic number system.

#include <stdint.h>
#include <math.h>

uint8_t hertz2fcode(uint64_t hertz)
{  // convert a frequency in hertz to a frequency code
	uint8_t code;

	if (hertz == 0)
		return 0;
	if (hertz < 20000)
		return 1;
	if (hertz > 270000000000)
		return 255;
	return (uint8_t)(0.5 + 15.47 * log(hertz / 18748.1));
}

uint64_t fcode2hertz(uint8_t code)
{  // convert a frequency code to a frequency in hertz
	if (code == 0)
		return 0;
	return (uint64_t)(0.5 + 18748.1 * exp(code / 15.47));
}

/*
#include <stdio.h>
int main()
{
	uint64_t freq = 7E6;
	uint16_t code;
	uint64_t decode;

	code = hertz2code(freq);
	decode = code2hertz(code);
	printf("freq %12.6f  code %4d decoded freq %12.6f\n", freq * 1E-6, code, decode * 1E-6);
	return 0;
}
*/
