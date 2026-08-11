if (NOT DEFINED CALCULATOR_EXE)
    message(FATAL_ERROR "CALCULATOR_EXE is required")
endif ()

if (NOT DEFINED CALCULATOR_TEST_WORK_DIR)
    set(CALCULATOR_TEST_WORK_DIR "${CMAKE_CURRENT_BINARY_DIR}")
endif ()

function(check_contains name output expected)
    string(FIND "${output}" "${expected}" found_at)
    if (found_at EQUAL -1)
        message(FATAL_ERROR
                "${name}: expected output to contain:\n${expected}\nActual output:\n${output}")
    endif ()
endfunction()

function(run_expression name expression expected)
    execute_process(
            COMMAND "${CALCULATOR_EXE}" -e "${expression}"
            RESULT_VARIABLE result
            OUTPUT_VARIABLE output
            ERROR_VARIABLE error_output)
    if (NOT result EQUAL 0)
        message(FATAL_ERROR
                "${name}: calculator exited with ${result}\nstdout:\n${output}\nstderr:\n${error_output}")
    endif ()

    check_contains("${name}" "${output}" "${expected}")
endfunction()

run_expression("simple arithmetic" "2 + 3 * 4" "Result: 14")
run_expression("number parser" "1.25e2 + .5" "Result: 125.5")
run_expression("real functions" "sqrt(9) + abs(-5)" "Result: 8")
run_expression("degree conversion" "sin(deg(90))" "Result: 1")
run_expression("log and tan" "log(1000) + tan(0)" "Result: 3")
run_expression("factorial" "5!" "Result: 120")
run_expression("complex sqrt" "sqrt(-1)" "Result: 0 + 1i")
run_expression("complex power" "(1+i)^2" "Result: 0 + 2i")
run_expression("fixed precision suffix" "pi:40"
               "Result: 3.1415926535897932384626433832795028841972")

set(session_file "${CALCULATOR_TEST_WORK_DIR}/calculator_cli_session.txt")
file(WRITE "${session_file}"
        "2 + 2\n"
        "ans * 3\n"
        "var total = 10\n"
        "total + 5\n"
        "total\n"
        "const p = pi\n"
        "p:40\n")

execute_process(
        COMMAND "${CALCULATOR_EXE}" --file "${session_file}"
        RESULT_VARIABLE session_result
        OUTPUT_VARIABLE session_output
        ERROR_VARIABLE session_error_output)
if (NOT session_result EQUAL 0)
    message(FATAL_ERROR
            "session: calculator exited with ${session_result}\nstdout:\n${session_output}\nstderr:\n${session_error_output}")
endif ()

check_contains("session first result" "${session_output}" "Result: 4")
check_contains("session ans result" "${session_output}" "Result: 12")
check_contains("session var assignment" "${session_output}" "Set var: total = 10")
check_contains("session var update result" "${session_output}" "Result: 15")
check_contains("session var update marker" "${session_output}" "Updated var: total = 15")
check_contains("session const assignment" "${session_output}" "Set const: p = 3.14159265358979")
check_contains("session refreshed const" "${session_output}"
               "Result: 3.1415926535897932384626433832795028841972")

message(STATUS "calculator CLI tests passed")
