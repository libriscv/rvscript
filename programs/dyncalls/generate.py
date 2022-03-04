import binascii
import json
from array import array

from argparse import ArgumentParser
parser = ArgumentParser()
parser.add_argument("-j", "--json", dest="jsonfile", default="dyncalls.json",
                    help="read JSON from FILE", metavar="FILE")
parser.add_argument("-o", "--output", dest="output",
                    help="write generated prototypes to FILE", metavar="FILE")
parser.add_argument("-v", "--verbose",
                    action="store_true", dest="verbose", default=False,
                    help="print status messages to stdout")
parser.add_argument("-P", "--print-sources",
                    action="store_true", dest="print_sources", default=False,
                    help="print generated sources to stdout")
parser.add_argument('--dyncall', dest='dyncall', action='store', default=104,
                   help='set the dyncall system call number')
parser.add_argument('--cpp', dest='cpp', action='store_true', default=False,
                   help='generate a .cpp file in addition to the .c file')

args = parser.parse_args()

# this is the plain CRC32 polynomial
poly = 0xEDB88320
# we need to be able to calculate CRC32 using any poly
table = array('L')
for byte in range(256):
    crc = 0
    for bit in range(8):
        if (byte ^ crc) & 1:
            crc = (crc >> 1) ^ poly
        else:
            crc >>= 1
        byte >>= 1
    table.append(crc)

def crc32(string):
    value = 0xffffffff
    for ch in string:
        value = table[(ord(ch) ^ value) & 0xff] ^ (value >> 8)

    return -1 - value

# load JSON
j = {}
with open(args.jsonfile) as f:
	j = json.load(f)

header = """
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

""";

# iterate typedefs first
for key in j:
	if key == "typedef":
		header += j[key] + ";\n"
header += "\n"

source = "";

# create dyncall prototypes and assembly
for key in j:
	if key != "typedef":
		asmdef  = j[key]
		asmname = asmdef.split(' ')[1]
		crc = '%08x' % (crc32(key) & 0xffffffff)
		if args.verbose:
			print(key + " dynamic call hash 0x" + crc)

		header += "// " + key + ": 0x" + crc + "\n";
		header += "extern " + asmdef + ";\n";
		source += 'asm("\\n\\\n'
		source += '.global ' + asmname + '\\n\\\n'
		source += '.func ' + asmname + '\\n\\\n'
		source += asmname + ':\\n\\\n'
		source += '  li t0, 0x' + crc + '\\n\\\n'
		source += '  la t1, ' + asmname + '_str\\n\\\n'
		source += '  li a7, ' + str(args.dyncall) + '\\n\\\n'
		source += '  ecall\\n\\\n'
		source += '  ret\\n\\\n'
		source += '.endfunc\\n\\\n'
		source += '.pushsection .rodata\\n\\\n'
		source += asmname + '_str:\\n\\\n'
		source += '.asciz \\\"' + key + '\\\"\\n\\\n'
		source += '.popsection\\n\\\n'
		source += '");\n\n'

header += """
#ifdef __cplusplus
}
#endif
""";

if (args.print_sources):
	print(header)
	print(source)

if (args.output):
	with open(args.output + ".h", "w") as hdrfile:
		hdrfile.write(header)
	with open(args.output + ".c", "w") as srcfile:
		srcfile.write(source)
	if args.cpp:
		with open(args.output + ".cpp", "w") as srcfile:
			srcfile.write(source)
	exit(0)
else:
	exit(1)
