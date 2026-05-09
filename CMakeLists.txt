cmake_minimum_required(VERSION 4.0)
project(calculator C)

set(CMAKE_C_STANDARD 11)

add_executable(calculator main.c
        math_parser.c
        math_parser.h)
