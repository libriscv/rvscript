proc start() {.cdecl, exportc.}
import json
import ffi

var j = %* {
    "name": "Hello",
    "email": "World",
    "books": ["Foundation"]
}

proc bench_nim() {.cdecl, exportc.} =
    discard j.pretty()

proc start() =
    benchmark("Testing Nim bencmarks", bench_nim)
