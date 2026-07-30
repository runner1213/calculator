cmake_minimum_required(VERSION 3.20)
project(calculator C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(calculator_core
        src/calculator.c
        src/calculator_mpfr.c
        src/lexer.c
        src/lexer.h
        src/lexer_mpfr.c
        src/lexer_mpfr.h
        src/parser.c
        src/parser.h
        src/parser_mpfr.c
        src/parser_mpfr.h
        include/calculator/calculator.h
        include/calculator/calculator_mpfr.h
        include/calculator/math_parser.h)

target_include_directories(calculator_core PUBLIC include PRIVATE src)

set(CALCULATOR_BUNDLED_MINGW_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/third_party/msys2-mingw64")
if (EXISTS "${CALCULATOR_BUNDLED_MINGW_PREFIX}/include/mpfr.h")
    list(PREPEND CMAKE_PREFIX_PATH "${CALCULATOR_BUNDLED_MINGW_PREFIX}")
    list(PREPEND CMAKE_INCLUDE_PATH "${CALCULATOR_BUNDLED_MINGW_PREFIX}/include")
    list(PREPEND CMAKE_LIBRARY_PATH "${CALCULATOR_BUNDLED_MINGW_PREFIX}/lib")
endif ()

find_package(PkgConfig QUIET)
if (PkgConfig_FOUND)
    pkg_check_modules(MPFR QUIET IMPORTED_TARGET mpfr)
endif ()

if (MPFR_FOUND)
    target_link_libraries(calculator_core PUBLIC PkgConfig::MPFR)
else ()
    find_path(MPFR_INCLUDE_DIR mpfr.h)
    find_library(MPFR_LIBRARY NAMES mpfr)
    find_library(GMP_LIBRARY NAMES gmp)
    find_file(MPFR_RUNTIME_DLL NAMES libmpfr-6.dll libmpfr.dll
            HINTS "${CALCULATOR_BUNDLED_MINGW_PREFIX}/bin")
    find_file(GMP_RUNTIME_DLL NAMES libgmp-10.dll libgmp.dll
            HINTS "${CALCULATOR_BUNDLED_MINGW_PREFIX}/bin")

    if (NOT MPFR_INCLUDE_DIR OR NOT MPFR_LIBRARY OR NOT GMP_LIBRARY)
        message(FATAL_ERROR "MPFR and GMP are required. Install mpfr/gmp or make them visible to CMake.")
    endif ()

    target_include_directories(calculator_core PUBLIC ${MPFR_INCLUDE_DIR})
    target_link_libraries(calculator_core PUBLIC ${MPFR_LIBRARY} ${GMP_LIBRARY})
endif ()

if (NOT MSVC)
    target_link_libraries(calculator_core PUBLIC m)
endif ()

add_executable(calculator src/main.c)
target_link_libraries(calculator PRIVATE calculator_core)

if (WIN32 AND MPFR_RUNTIME_DLL AND GMP_RUNTIME_DLL)
    add_custom_command(TARGET calculator POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${MPFR_RUNTIME_DLL}"
                    "${GMP_RUNTIME_DLL}"
                    "$<TARGET_FILE_DIR:calculator>")
endif ()
