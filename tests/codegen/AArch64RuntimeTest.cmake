if(NOT DEFINED COMPILER OR NOT DEFINED AARCH64_CC OR
   NOT DEFINED QEMU_AARCH64 OR NOT DEFINED SOURCE OR
   NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "AArch64 runtime test is missing configuration")
endif()

file(MAKE_DIRECTORY "${WORK_DIR}")
set(OBJECT "${WORK_DIR}/program.o")
set(EXECUTABLE "${WORK_DIR}/program")

execute_process(
    COMMAND "${COMPILER}" "${SOURCE}"
            --target=aarch64-unknown-linux-gnu --cpu=generic
            --emit-obj -O2 -o "${OBJECT}"
    RESULT_VARIABLE COMPILE_RESULT
    ERROR_VARIABLE COMPILE_ERROR
)
if(NOT COMPILE_RESULT EQUAL 0)
    message(FATAL_ERROR "AArch64 object generation failed: ${COMPILE_ERROR}")
endif()

execute_process(
    COMMAND "${AARCH64_CC}" "${OBJECT}" -o "${EXECUTABLE}"
    RESULT_VARIABLE LINK_RESULT
    ERROR_VARIABLE LINK_ERROR
)
if(NOT LINK_RESULT EQUAL 0)
    message(FATAL_ERROR "AArch64 cross-link failed: ${LINK_ERROR}")
endif()

if(DEFINED AARCH64_SYSROOT AND NOT AARCH64_SYSROOT STREQUAL "")
    execute_process(
        COMMAND "${QEMU_AARCH64}" -L "${AARCH64_SYSROOT}" "${EXECUTABLE}"
        RESULT_VARIABLE EXECUTION_RESULT
        ERROR_VARIABLE EXECUTION_ERROR
    )
else()
    execute_process(
        COMMAND "${QEMU_AARCH64}" "${EXECUTABLE}"
        RESULT_VARIABLE EXECUTION_RESULT
        ERROR_VARIABLE EXECUTION_ERROR
    )
endif()

if(NOT EXECUTION_RESULT EQUAL 13)
    message(FATAL_ERROR
        "expected AArch64 exit value 13, got ${EXECUTION_RESULT}: "
        "${EXECUTION_ERROR}"
    )
endif()
