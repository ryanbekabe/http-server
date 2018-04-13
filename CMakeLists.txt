cmake_minimum_required(VERSION 3.10)
project(bserver C)

set(CMAKE_C_STANDARD 99)

add_executable(bserver main.c iniparse.c iniparse.h common.c common.h)