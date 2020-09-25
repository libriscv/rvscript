import strformat
from ffi import print
proc nim_test() {.exportc: "nim_test", used.}

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
