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

hashlist = []

def crc32(string):
    value = 0xffffffff
    for ch in string:
        value = table[(ord(ch) ^ value) & 0xff] ^ (value >> 8)

    return -1 - value

def find_arguments(string):
	sfront = string.split('(', 1)
	retval = [sfront[0].split(' ')[0]]
	strargs = sfront[1].split(')')[0]
	# [retval, arg0, arg1, '']
	fargs = retval + strargs.split(", ")
	# Remove empty argument lists
	if fargs[-1] == "":
		fargs.pop()
	print(fargs)
	return fargs

def hash_index_of(crc):
	crcval = int(crc, 16)
	return hashlist.index(crcval)

def emit_inline_assembly(header, asmdef, crc, fargs):
	retval = fargs[0]
	fargs.pop(0)

	has_output = (retval != "void")
	inputs = []
	output = []
	areg = 0
	asm_regs = ""
	asm_in   = []
	asm_out  = []
	asm_clob = []

	if has_output:
		asm_regs += "register " + retval + " ra0 asm(\"a0\");\n"
		asm_out = ["\"=r\"(ra0)"]

	#asm_regs += "register const char* t0 asm(\"t1\") = \"" + key + "\";\n"
	#asm_regs += "register uint32_t t0 asm(\"t0\") = 0x" + crc + ";\n"
	#asm_regs += "register uint32_t a7 asm(\"a7\") = 0x" + crc + ";\n"
	asm_regs += "register int a7 asm(\"a7\") = 400 + " + str(hash_index_of(crc)) + ";\n"
	asm_clob += []

	for arg in fargs:
		reg = "a" + str(areg)
		asm_regs += "register " + arg + " " + reg + " asm(\"" + reg + "\") = arg" + str(areg) + ";\n"
		# strings
		if arg == "char*" or arg == "char *" or arg == "const char*" or arg == "const char *":
			asm_in += ["\"r\"(" + reg + ")"]
			asm_in += ["\"m\"(*" + reg + ")"]
		# integrals
		else:
			asm_in += ["\"r\"(" + reg + ")"]
		fargs[areg ] = arg + " arg" + str(areg)
		areg += 1

	asm_in += ["\"r\"(a7)"] #, "\"r\"(t0)"]

	header += "static inline " + retval + " i" + asmdef + " (" + ','.join(fargs) + ') {\n'
	header += asm_regs
	header += "__asm__(\"ecall\" : " + ",".join(asm_out) + " : " + ",".join(asm_in) + " : " + ",".join(asm_clob) + ");\n"
	if has_output:
		header += "return ra0;\n"
	header += "}" + '\n'
	return header

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
	else:
		crcval = crc32(key) & 0xffffffff
		hashlist += [crcval]

header += "\n"

# the sorted hashes are used for faster dynamic calls
# (which has a fallback to normal hash lookup)
hashlist.sort()		

source = '__asm__(".section .text\\n");\n\n'

hashlist_source = '__asm__("\\n\\\n'
hashlist_source += '.global dyncall_hashcnt\\n\\\n'
hashlist_source += '.global dyncall_hashlist\\n\\\n'
hashlist_source += '.pushsection .rodata\\n\\\n'
hashlist_source += 'dyncall_hashcnt:\\n\\\n'
hashlist_source += '  .word ' + str(len(hashlist)) + '\\n\\\n'
hashlist_source += 'dyncall_hashlist:\\n\\\n'
for crcval in hashlist:
	crc = '%08x' % crcval
	hashlist_source += '  .word 0x' + crc + '\\n\\\n'

# create dyncall prototypes and assembly
for key in j:
	if key != "typedef":
		asmdef  = j[key]
		asmname = asmdef.split(' ')[1]

		fargs = find_arguments(asmdef)

		## CRC32 cannot be < 600, as that would
		## collide with other system call numbers
		crcval = crc32(key) & 0xffffffff
		if crcval < 600:
			print("ERROR: Dynamic call '" + key + "' has a collision, ignored!")
			continue
		crc = '%08x' % crcval
		if args.verbose:
			print(key + " dynamic call hash 0x" + crc)

		header += "// " + key + ": 0x" + crc + "\n"
		header += "extern " + asmdef + ";\n"

		## Given the parsed arguments, starting with
		## the return value, we can produce perfect
		## inline assembly that allows the compiler
		## room to optimize better.
		header = emit_inline_assembly(header, asmname, crc, fargs)

		source += '__asm__("\\n\\\n'
		source += '.global ' + asmname + '\\n\\\n'
		source += '.func ' + asmname + '\\n\\\n'
		source += asmname + ':\\n\\\n'
		source += '  li t0, 0x' + crc + '\\n\\\n'
		source += '  lui t1, %hi(' + asmname + '_str)\\n\\\n'
		source += '  addi t1, t1, %lo(' + asmname + '_str)\\n\\\n'
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

hashlist_source += '.popsection\\n\\\n'
hashlist_source += '");\n\n'

if (args.print_sources):
	print(header)
	print(source)

if (args.output):
	with open(args.output + ".h", "w") as hdrfile:
		hdrfile.write(header)
	with open(args.output + ".c", "w") as srcfile:
		srcfile.write(source)
		srcfile.write(hashlist_source)
	if args.cpp:
		with open(args.output + ".cpp", "w") as srcfile:
			srcfile.write(source)
			srcfile.write(hashlist_source)
	exit(0)
else:
	exit(1)
