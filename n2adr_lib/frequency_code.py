# This is firmware for the Hermes Lite 2 IO board designed by Jim Ahlstrom, N2ADR. It is
#   Copyright (c) 2022-2023 James C. Ahlstrom <jahlstr@gmail.com>.
#   It is licensed under the MIT license. See MIT.txt.

# Thanks to Clifford Heath for suggesting a logarithmic number system.

import math

def hertz2code(freq):
  if freq == 0:
    return 0
  code = int(0.5 + 15.47 * math.log(freq / 18748.1))
  if code < 1:
    return 1
  elif code > 255:
    return 255
  return code

def code2hertz(code):
  if code == 0:
    return 0
  freq = int(0.5 + 18748.1 * math.exp(code / 15.47))
  return freq

# Run this file as a main program to print out a table of codes.
if __name__ == "__main__":
  BandEdge = {
	'137k':( 135700,   137800),	'500k':( 472000,   479000),
	'160':( 1800000,  2000000),	'80' :( 3500000,  4000000),
	'60' :( 5300000,  5430000),	'40' :( 7000000,  7300000),
	'30' :(10100000, 10150000),	'20' :(14000000, 14350000),	
	'17' :(18068000, 18168000), '15' :(21000000, 21450000),
	'12' :(24890000, 24990000),	'10' :(28000000, 29700000),
	'6'     :(  50000000,   54000000),
	'4'     :(  70000000,   70500000),
	'2'     :( 144000000,  148000000),
    '1.25'  :( 222000000,  225000000),
    '70cm'  :( 420000000,  450000000),
    '33cm'  :( 902000000,  928000000),
    '23cm'  :(1240000000, 1300000000),
    '13cm'  :(2300000000, 2450000000),
    '9cm'   :(3300000000, 3500000000),
    '5cm'   :(5650000000, 5925000000),
    '3cm'  :(10000000000,10500000000),
	}
  print (" Band         F1           F2    Code1  Code2      Decode1      Decode2")
  for key in BandEdge:
    f1, f2 = BandEdge[key]
    code1 = hertz2code(f1)
    code2 = hertz2code(f2)
    ff1 = code2hertz(code1)
    ff2 = code2hertz(code2)
    print ("%5s %12.6f %12.6f %6d %6d %12.6f %12.6f" % (
             key, f1 * 1E-6, f2 * 1E-6, code1, code2, ff1 * 1E-6, ff2 * 1E-6))
  print()
  print ("Code and decoded frequency in MHz")
  table = [''] * 52
  row = 0
  codes = [0, 1] + list(range(30, 197)) + [255]
  for code1 in range(0, 256):
    freq = code2hertz(code1)
    table[row] += " %3d %13f   " % (code1, freq * 1E-6)
    row += 1
    if row % 52 == 0:
      row = 0
  for row in table:
    print(row)

