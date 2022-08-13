
proc console_print(text: cstring, tlen: int) {.importc, header: "ffi.h"}
proc exit(status: int) {.importc, header: "ffi.h"}
proc breakpoint*(message: cstring = "") {.importc, header: "ffi.h"}

type measure_cb* = proc() {.cdecl.}
proc sys_measure(name: cstring, callback: measure_cb) {.importc, header: "ffi.h"}

proc print*(content: string) =
    console_print(content, len(content))

proc exit*() {.used.} =
    exit(-1)

proc benchmark*(name: string, callback: measure_cb) =
    sys_measure(cstring(name), callback)
