# Copyright (C) 2026 Oscar Priego Verdugo
# SPDX-License-Identifier: GPL-3.0-only

if(NOT DEFINED COMPILER OR NOT DEFINED LLVM_READOBJ OR
   NOT DEFINED SOURCE OR NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "cross-target generation test is missing configuration")
endif()

file(MAKE_DIRECTORY "${WORK_DIR}")

foreach(TARGET_NAME x86_64 aarch64)
    if(TARGET_NAME STREQUAL "x86_64")
        set(TARGET_TRIPLE x86_64-unknown-linux-gnu)
        set(EXPECTED_MACHINE EM_X86_64)
    else()
        set(TARGET_TRIPLE aarch64-unknown-linux-gnu)
        set(EXPECTED_MACHINE EM_AARCH64)
    endif()

    set(ASSEMBLY "${WORK_DIR}/${TARGET_NAME}.s")
    set(OBJECT "${WORK_DIR}/${TARGET_NAME}.o")
    set(LLVM_IR "${WORK_DIR}/${TARGET_NAME}.ll")

    execute_process(
        COMMAND "${COMPILER}" "${SOURCE}"
                --target=${TARGET_TRIPLE} --cpu=generic
                --emit-asm -O2 -o "${ASSEMBLY}"
        RESULT_VARIABLE ASSEMBLY_RESULT
        ERROR_VARIABLE ASSEMBLY_ERROR
    )
    if(NOT ASSEMBLY_RESULT EQUAL 0)
        message(FATAL_ERROR
            "${TARGET_NAME} assembly generation failed: ${ASSEMBLY_ERROR}"
        )
    endif()

    execute_process(
        COMMAND "${COMPILER}" "${SOURCE}"
                --target=${TARGET_TRIPLE} --cpu=generic
                --emit-obj -O2 -o "${OBJECT}"
        RESULT_VARIABLE OBJECT_RESULT
        ERROR_VARIABLE OBJECT_ERROR
    )
    if(NOT OBJECT_RESULT EQUAL 0)
        message(FATAL_ERROR
            "${TARGET_NAME} object generation failed: ${OBJECT_ERROR}"
        )
    endif()

    execute_process(
        COMMAND "${COMPILER}" "${SOURCE}"
                --target=${TARGET_TRIPLE}
                --emit-llvm-before-opt -o "${LLVM_IR}"
        RESULT_VARIABLE IR_RESULT
        ERROR_VARIABLE IR_ERROR
    )
    if(NOT IR_RESULT EQUAL 0)
        message(FATAL_ERROR
            "${TARGET_NAME} target IR generation failed: ${IR_ERROR}"
        )
    endif()

    execute_process(
        COMMAND "${LLVM_READOBJ}" --file-headers "${OBJECT}"
        RESULT_VARIABLE READOBJ_RESULT
        OUTPUT_VARIABLE READOBJ_OUTPUT
        ERROR_VARIABLE READOBJ_ERROR
    )
    if(NOT READOBJ_RESULT EQUAL 0)
        message(FATAL_ERROR
            "${TARGET_NAME} object inspection failed: ${READOBJ_ERROR}"
        )
    endif()
    if(NOT READOBJ_OUTPUT MATCHES "${EXPECTED_MACHINE}")
        message(FATAL_ERROR
            "${TARGET_NAME} object has unexpected ELF machine:\n${READOBJ_OUTPUT}"
        )
    endif()

    file(SIZE "${ASSEMBLY}" ASSEMBLY_SIZE)
    file(SIZE "${OBJECT}" OBJECT_SIZE)
    if(ASSEMBLY_SIZE EQUAL 0 OR OBJECT_SIZE EQUAL 0)
        message(FATAL_ERROR "${TARGET_NAME} generated an empty artifact")
    endif()

    file(READ "${LLVM_IR}" LLVM_IR_TEXT)
    if(NOT LLVM_IR_TEXT MATCHES "target triple = \"${TARGET_TRIPLE}\"")
        message(FATAL_ERROR "${TARGET_NAME} LLVM IR has the wrong target triple")
    endif()
    if(NOT LLVM_IR_TEXT MATCHES "target datalayout = \"")
        message(FATAL_ERROR "${TARGET_NAME} LLVM IR has no target DataLayout")
    endif()
endforeach()
