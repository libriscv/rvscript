cmake_minimum_required(VERSION 3.5)
project(modbuilder CXX)

add_subdirectory(dyncalls)
include(micro/micro.cmake)

add_subdirectory ("${ENGINE_PATH}/scripts" mod)
