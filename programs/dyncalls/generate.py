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
parser.add_argument('--dyncall', dest='dyncall', action='store', default=504,
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

def is_type(string):
	keywords = ["unsigned", "char", "short", "int", "long", "float", "double", "size_t", "int8_t", "uint8_t", "int16_t", "uint16_t", "int32_t", "uint32_t", "int64_t", "uint64_t"]
	conventions = ("_callback", "_t")
	return ("*" in string) or string in keywords or string.endswith(conventions)

def is_simple_return(string):
	keywords = ["*", "void", "unsigned", "char", "short", "int", "long", "size_t", "int8_t", "uint8_t", "int16_t", "uint16_t", "int32_t", "uint32_t", "int64_t", "uint64_t"]
	return any(substring in string for substring in keywords)

def find_arguments(string):
	sfront = string.split('(', 1)
	retval = [sfront[0].split(' ')[0]]
	strargs = sfront[1].split(')')[0]
	# [retval, arg0, arg1, '']
	fargs = retval + strargs.split(", ")
	# Remove parameter names
	for (idx, arg) in enumerate(fargs):
		symbols = arg.split(" ")
		if len(symbols) > 1:
			last = symbols[-1]
			if not is_type(last):
				symbols.pop()
				fargs[idx] = " ".join(symbols)
	# Remove empty argument lists
	if fargs[-1] == "":
		fargs.pop()
	return fargs

def emit_inline_assembly(header, asmdef, index, fargs):
	retval = fargs[0]
	fargs.pop(0)

	if not is_simple_return(retval):
		return (header, False)

	has_output = (retval != "void")
	inputs = []
	output = []
	argno = 0
	areg = 0
	freg = 0
	asm_regs = ""
	asm_in   = []
	asm_out  = []
	asm_clob = []

	if has_output:
		asm_regs += "register " + retval + " ra0 __asm__(\"a0\");\n"
		asm_out = ["\"=r\"(ra0)"]

	asm_clob += []

	for arg in fargs:
		if "float" in arg or "double" in arg:
			# floating-point registers
			reg = "fa" + str(freg)
			freg += 1
		else:
			# integral registers
			reg = "a" + str(areg)
			areg += 1
		asm_regs += "register " + arg + " " + reg + " __asm__(\"" + reg + "\") = arg" + str(argno) + ";\n"
		# strings
		if "*" in arg:
			asm_in += ["\"r\"(" + reg + ")"]
			if "void" in arg:
				asm_in += ["\"m\"(*(char *)arg" + str(argno) + ")"]
			else:
				asm_in += ["\"m\"(*arg" + str(argno) + ")"]
		# floats
		elif "float" in arg or "double" in arg:
			asm_in += ["\"f\"(" + reg + ")"]
		# integrals
		else:
			asm_in += ["\"r\"(" + reg + ")"]
		fargs[argno] = arg + " arg" + str(argno)
		argno += 1

	asm_in += []

	header += "static inline __attribute__((always_inline, optimize(\"O2\"))) " + retval + " i" + asmdef + " (" + ','.join(fargs) + ') {\n'
	header += asm_regs
	header += '__asm__ volatile(\".insn i 0b1011011, 0, x0, x0, ' + str(index) + "\"" \
		+ " : " + ",".join(asm_out) + " : " + ",".join(asm_in) + " : " + ",".join(asm_clob) + ");\n"
	if has_output:
		header += "return ra0;\n"
	header += "}" + '\n'
	return (header, True)

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
		for typedef in j[key]:
			header += typedef + ";\n"
header += "\n"

source = '__asm__(".section .text\\n");\n\n'
dyncallindex = 0

dyncall = ""

# create dyncall prototypes and assembly
for key in j:
	if key != "typedef":
		asmdef  = j[key]
		asmname = asmdef.split(' ')[1]

		fargs = find_arguments(asmdef)

		crcval = crc32(asmdef) & 0xffffffff
		crc = '%08x' % crcval

		header += "// " + key + ": 0x" + crc + "\n"
		header += "extern " + asmdef + ";\n"

		## Given the parsed arguments, starting with
		## the return value, we can produce perfect
		## inline assembly that allows the compiler
		## room to optimize better.
		(header, inlined) = emit_inline_assembly(header, asmname, dyncallindex, fargs)

		if args.verbose:
			print("Dynamic call: " + key + ", hash 0x" + crc + (" (inlined)" if inlined else ""))

		dyncall += '  .long 0x' + crc + '\\n\\\n'
		dyncall += '  .long ' + str(0) + '\\n\\\n'
		dyncall += '  .long ' + asmname + '_str\\n\\\n'

		# These dynamic calls use the table indexed variant
		# Each dynamic call has a table index where the name and hash is stored
		# and at run-time this value is lazily resolved
		source += '__asm__("\\n\\\n'
		source += '.global ' + asmname + '\\n\\\n'
		source += '.func ' + asmname + '\\n\\\n'
		source += asmname + ':\\n\\\n'
		source += '  .insn i 0b1011011, 0, x0, x0, ' + str(dyncallindex) + '\\n\\\n'
		source += '  ret\\n\\\n'
		source += '.endfunc\\n\\\n'
		source += '.pushsection .rodata\\n\\\n'
		source += asmname + '_str:\\n\\\n'
		source += '.asciz \\\"' + key + '\\\"\\n\\\n'
		source += '.popsection\\n\\\n'
		source += '");\n\n'

		dyncallindex += 1

header += """
#ifdef __cplusplus
}
#endif
"""

dyncall_header =  '__asm__("\\n\\\n'
dyncall_header += '.pushsection .rodata\\n\\\n'
dyncall_header += '.align 8\\n\\\n'
dyncall_header += '.global dyncall_table\\n\\\n'
dyncall_header += 'dyncall_table:\\n\\\n'
dyncall_header += '  .long ' + str(dyncallindex) + '\\n\\\n'

dyncall = dyncall_header + dyncall
dyncall += '.popsection\\n\\\n'
dyncall += '");\n\n'

source += dyncall

if (args.verbose):
	print("* There are " + str(dyncallindex) + " dynamic calls")

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
