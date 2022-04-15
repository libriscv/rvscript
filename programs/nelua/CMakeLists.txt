cmake_minimum_required(VERSION 3.11.0)
project(programs C)

include(nelua.cmake)

add_nelua_binary(hello_nelua
	src/default.symbols
	main.nelua.c
)
