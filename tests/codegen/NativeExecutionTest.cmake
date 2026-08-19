if(NOT DEFINED COMPILER OR NOT DEFINED LINKER OR
   NOT DEFINED SOURCE_VALUE OR NOT DEFINED EXPECTED OR
   NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "native execution test is missing configuration")
endif()

if(NOT DEFINED OPT_LEVEL)
    set(OPT_LEVEL -O0)
endif()

file(MAKE_DIRECTORY "${WORK_DIR}")
set(SOURCE "${WORK_DIR}/branch-${SOURCE_VALUE}.oc")
set(OBJECT "${WORK_DIR}/branch-${SOURCE_VALUE}.o")
set(EXECUTABLE "${WORK_DIR}/branch-${SOURCE_VALUE}")

file(WRITE "${SOURCE}"
    "X = ${SOURCE_VALUE}\n"
    "if X > 10 { Y = X + 1 } else { Y = X - 1 }\n"
    "return Y\n"
)

execute_process(
    COMMAND "${COMPILER}" "${SOURCE}" --emit-obj "${OPT_LEVEL}" -o "${OBJECT}"
    RESULT_VARIABLE COMPILE_RESULT
    ERROR_VARIABLE COMPILE_ERROR
)
if(NOT COMPILE_RESULT EQUAL 0)
    message(FATAL_ERROR "object generation failed: ${COMPILE_ERROR}")
endif()

execute_process(
    COMMAND "${LINKER}" "${OBJECT}" -o "${EXECUTABLE}"
    RESULT_VARIABLE LINK_RESULT
    ERROR_VARIABLE LINK_ERROR
)
if(NOT LINK_RESULT EQUAL 0)
    message(FATAL_ERROR "native link failed: ${LINK_ERROR}")
endif()

execute_process(
    COMMAND "${EXECUTABLE}"
    RESULT_VARIABLE EXECUTION_RESULT
)
if(NOT EXECUTION_RESULT EQUAL EXPECTED)
    message(FATAL_ERROR
        "expected exit value ${EXPECTED}, got ${EXECUTION_RESULT}"
    )
endif()
