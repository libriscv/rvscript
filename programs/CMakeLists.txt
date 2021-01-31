cmake_minimum_required(VERSION 3.1.0)
project(modbuilder CXX)

include(micro/micro.cmake)

#
# Mod settings
#
set (MODNAME "hello_world" CACHE STRING "Mod name")
set (MODPATH "${ENGINE_PATH}/mods/${MODNAME}")

add_subdirectory (${MODPATH} mod)
