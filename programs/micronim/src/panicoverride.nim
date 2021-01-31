import ffi
{.push stack_trace: off, profiler:off.}

proc rawoutput(s: string) =
  printcons(s)

proc panic(s: string) =
  printcons(s)
  exit()

{.pop.}
