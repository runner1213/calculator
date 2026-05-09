cmake_minimum_required(VERSION 3.20)
project(calculator C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(calculator_core
        src/calculator.c
        src/lexer.c
        src/lexer.h
        src/parser.c
        src/parser.h
        include/calculator/calculator.h
        include/calculator/math_parser.h)

target_include_directories(calculator_core PUBLIC include PRIVATE src)

if (NOT MSVC)
    target_link_libraries(calculator_core PUBLIC m)
endif ()

add_executable(calculator src/main.c)
target_link_libraries(calculator PRIVATE calculator_core)
