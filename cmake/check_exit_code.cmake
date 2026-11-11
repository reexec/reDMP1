# check_exit_code.cmake
# Invoked via: cmake -DCMD=<executable> -DARGS="<arg1>;<arg2>..." -DEXPECTED_CODE=<codeNum> -P cmake/check_exit_code.cmake

if(NOT DEFINED CMD)
    message(FATAL_ERROR "CMD not defined")
endif()

if(NOT DEFINED EXPECTED_CODE)
    message(FATAL_ERROR "EXPECTED_CODE not defined")
endif()

execute_process(
    COMMAND "${CMD}" ${ARGS}
    RESULT_VARIABLE actual_code
    OUTPUT_VARIABLE out_msg
    ERROR_VARIABLE err_msg
)

if(NOT actual_code EQUAL EXPECTED_CODE)
    message(FATAL_ERROR "Command '${CMD} ${ARGS}' exited with code ${actual_code}, expected ${EXPECTED_CODE}.\nStdout: ${out_msg}\nStderr: ${err_msg}")
endif()

if(DEFINED EXPECTED_REGEX)
    set(combined_msg "${out_msg}\n${err_msg}")
    if(NOT combined_msg MATCHES "${EXPECTED_REGEX}")
        message(FATAL_ERROR "Output did not match '${EXPECTED_REGEX}'.\nStdout: ${out_msg}\nStderr: ${err_msg}")
    endif()
endif()
