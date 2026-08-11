cmake_minimum_required(VERSION 3.20)
project(calculator C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(CALCULATOR_STATIC_MPFR "Link MPFR and GMP statically when using MinGW on Windows" OFF)

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

set(CALCULATOR_USE_STATIC_MPFR OFF)
if (CALCULATOR_STATIC_MPFR AND WIN32 AND MINGW)
    set(CALCULATOR_USE_STATIC_MPFR ON)
endif ()

if (NOT CALCULATOR_USE_STATIC_MPFR)
    find_package(PkgConfig QUIET)
    if (PkgConfig_FOUND)
        pkg_check_modules(MPFR QUIET IMPORTED_TARGET mpfr)
    endif ()
endif ()

if (MPFR_FOUND AND NOT CALCULATOR_USE_STATIC_MPFR)
    target_link_libraries(calculator_core PUBLIC PkgConfig::MPFR)
else ()
    find_path(MPFR_INCLUDE_DIR mpfr.h)
    if (CALCULATOR_USE_STATIC_MPFR)
        find_library(MPFR_STATIC_LIBRARY NAMES libmpfr.a
                HINTS "${CALCULATOR_BUNDLED_MINGW_PREFIX}/lib"
                NO_DEFAULT_PATH)
        find_library(GMP_STATIC_LIBRARY NAMES libgmp.a
                HINTS "${CALCULATOR_BUNDLED_MINGW_PREFIX}/lib"
                NO_DEFAULT_PATH)
        set(MPFR_LINK_LIBRARY "${MPFR_STATIC_LIBRARY}")
        set(GMP_LINK_LIBRARY "${GMP_STATIC_LIBRARY}")
    else ()
        find_library(MPFR_DYNAMIC_LIBRARY NAMES mpfr)
        find_library(GMP_DYNAMIC_LIBRARY NAMES gmp)
        set(MPFR_LINK_LIBRARY "${MPFR_DYNAMIC_LIBRARY}")
        set(GMP_LINK_LIBRARY "${GMP_DYNAMIC_LIBRARY}")

        get_filename_component(CALCULATOR_C_COMPILER_DIR "${CMAKE_C_COMPILER}" DIRECTORY)
        find_file(MPFR_RUNTIME_DLL NAMES libmpfr-6.dll libmpfr.dll
                HINTS "${CALCULATOR_BUNDLED_MINGW_PREFIX}/bin")
        find_file(GMP_RUNTIME_DLL NAMES libgmp-10.dll libgmp.dll
                HINTS "${CALCULATOR_BUNDLED_MINGW_PREFIX}/bin")
        find_file(MINGW_GCC_RUNTIME_DLL NAMES libgcc_s_seh-1.dll
                HINTS "${CALCULATOR_C_COMPILER_DIR}")
        find_file(MINGW_WINPTHREAD_RUNTIME_DLL NAMES libwinpthread-1.dll
                HINTS "${CALCULATOR_C_COMPILER_DIR}")
    endif ()

    if (NOT MPFR_INCLUDE_DIR OR NOT MPFR_LINK_LIBRARY OR NOT GMP_LINK_LIBRARY)
        message(FATAL_ERROR "MPFR and GMP are required. Install mpfr/gmp or make them visible to CMake.")
    endif ()

    target_include_directories(calculator_core SYSTEM PUBLIC ${MPFR_INCLUDE_DIR})
    if (CALCULATOR_USE_STATIC_MPFR)
        target_compile_definitions(calculator_core PUBLIC GMP_STATICLIB)
    endif ()
    target_link_libraries(calculator_core PUBLIC ${MPFR_LINK_LIBRARY} ${GMP_LINK_LIBRARY})
endif ()

if (NOT MSVC)
    target_link_libraries(calculator_core PUBLIC m)
endif ()

add_executable(calculator src/main.c)
target_link_libraries(calculator PRIVATE calculator_core)

enable_testing()

add_test(NAME calculator_cli_tests
        COMMAND ${CMAKE_COMMAND}
                -D "CALCULATOR_EXE=$<TARGET_FILE:calculator>"
                -D "CALCULATOR_TEST_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_tests.cmake")

if (CALCULATOR_USE_STATIC_MPFR)
    target_link_options(calculator PRIVATE -static-libgcc -Wl,--disable-dynamicbase)
elseif (WIN32)
    if (NOT MPFR_RUNTIME_DLL OR NOT GMP_RUNTIME_DLL OR NOT MINGW_GCC_RUNTIME_DLL OR NOT MINGW_WINPTHREAD_RUNTIME_DLL)
        message(FATAL_ERROR "MPFR, GMP, and MinGW runtime DLLs are required for a dynamic Windows build.")
    endif ()

    add_custom_command(TARGET calculator POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${MPFR_RUNTIME_DLL}"
                    "${GMP_RUNTIME_DLL}"
                    "${MINGW_GCC_RUNTIME_DLL}"
                    "${MINGW_WINPTHREAD_RUNTIME_DLL}"
                    "$<TARGET_FILE_DIR:calculator>")
endif ()
