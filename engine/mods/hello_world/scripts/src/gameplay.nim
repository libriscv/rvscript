import strformat
proc sys_print(data: cstring, len: cint) {.importcpp: "sys_print(@)", header: "ffi.h".}
proc nim_test() {.exportc: "nim_test", used.}

proc print(data: string) =
    sys_print(data.cstring, data.len.cint)

type
  Person = object
    name: string
    age: Natural # Ensures the age is positive

let people = [
    Person(name: "John", age: 45),
    Person(name: "Kate", age: 30)
]

proc nim_test() =
  print("Hello from Nim!\n")
  for person in people:
    # Type-safe string interpolation,
    # evaluated at compile time.
    print(fmt("{person.name} is {person.age} years old\n"))
