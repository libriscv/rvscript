proc hello_nim() {.cdecl, exportc.}
proc bench_nim() {.cdecl, exportc.}
import json
import ffi

var j = %* {
    "name": "Hello",
    "email": "World",
    "books": ["Foundation"]
}

proc hello_nim() =
    print "Before debugging\n"
    remote_breakpoint()
    print "Hello Nim World!\n" & j.pretty() & "\n"

proc bench_nim() =
    print "Hello Nim World!\n" & j.pretty() & "\n"
