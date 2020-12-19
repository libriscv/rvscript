
proc console_print(text: cstring, tlen: int) {.importc, header: "ffi.h"}
proc exit(status: int) {.importc, header: "ffi.h"}

proc printcons*(content: string) =
    console_print(content, len(content)-1)

proc exit*() {.used.} =
    exit(-1)
