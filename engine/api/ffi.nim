proc sys_print(data: cstring, len: cint) {.importcpp: "sys_print(@)", header: "ffi.h".}

proc print*(data: string) =
    sys_print(data.cstring, data.len.cint)
