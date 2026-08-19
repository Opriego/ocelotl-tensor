# Copyright (C) 2026 Oscar Priego Verdugo
# SPDX-License-Identifier: GPL-3.0-only

if(NOT DEFINED COMPILER OR NOT DEFINED LINKER OR
   NOT DEFINED RUNTIME_SHARED OR NOT DEFINED RUNTIME_STATIC OR
   NOT DEFINED RUNTIME_DIR OR NOT DEFINED NM OR NOT DEFINED READELF OR
   NOT DEFINED SOURCE OR NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "runtime ABI test is missing configuration")
endif()

file(MAKE_DIRECTORY "${WORK_DIR}")
set(LLVM_IR "${WORK_DIR}/runtime.ll")
set(OBJECT "${WORK_DIR}/runtime.o")
set(DYNAMIC_EXECUTABLE "${WORK_DIR}/runtime-dynamic")
set(STATIC_EXECUTABLE "${WORK_DIR}/runtime-static")

execute_process(
    COMMAND "${COMPILER}" "${SOURCE}" --emit-llvm-before-opt -o "${LLVM_IR}"
    RESULT_VARIABLE IR_RESULT ERROR_VARIABLE IR_ERROR)
if(NOT IR_RESULT EQUAL 0)
    message(FATAL_ERROR "runtime LLVM IR generation failed: ${IR_ERROR}")
endif()
file(READ "${LLVM_IR}" IR_TEXT)
foreach(SYMBOL ocelotl_rt_v1_alloc ocelotl_rt_v1_free)
    if(NOT IR_TEXT MATCHES "${SYMBOL}")
        message(FATAL_ERROR "generated LLVM IR does not call ${SYMBOL}")
    endif()
endforeach()

execute_process(
    COMMAND "${COMPILER}" "${SOURCE}" --emit-obj -O2 -o "${OBJECT}"
    RESULT_VARIABLE OBJECT_RESULT ERROR_VARIABLE OBJECT_ERROR)
if(NOT OBJECT_RESULT EQUAL 0)
    message(FATAL_ERROR "runtime object generation failed: ${OBJECT_ERROR}")
endif()

execute_process(
    COMMAND "${NM}" -u "${OBJECT}"
    RESULT_VARIABLE NM_OBJECT_RESULT OUTPUT_VARIABLE NM_OBJECT_OUTPUT
    ERROR_VARIABLE NM_OBJECT_ERROR)
if(NOT NM_OBJECT_RESULT EQUAL 0 OR
   NOT NM_OBJECT_OUTPUT MATCHES "ocelotl_rt_v1_alloc" OR
   NOT NM_OBJECT_OUTPUT MATCHES "ocelotl_rt_v1_free")
    message(FATAL_ERROR
        "object runtime imports are incorrect: ${NM_OBJECT_ERROR}\n${NM_OBJECT_OUTPUT}")
endif()

execute_process(
    COMMAND "${LINKER}" "${OBJECT}" "${RUNTIME_SHARED}"
            -Wl,-rpath,"${RUNTIME_DIR}" -o "${DYNAMIC_EXECUTABLE}"
    RESULT_VARIABLE DYNAMIC_LINK_RESULT ERROR_VARIABLE DYNAMIC_LINK_ERROR)
if(NOT DYNAMIC_LINK_RESULT EQUAL 0)
    message(FATAL_ERROR "dynamic runtime link failed: ${DYNAMIC_LINK_ERROR}")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
            "LD_LIBRARY_PATH=${RUNTIME_DIR}" "${DYNAMIC_EXECUTABLE}"
    RESULT_VARIABLE DYNAMIC_RUN_RESULT ERROR_VARIABLE DYNAMIC_RUN_ERROR)
if(NOT DYNAMIC_RUN_RESULT EQUAL 7)
    message(FATAL_ERROR
        "dynamic runtime execution returned ${DYNAMIC_RUN_RESULT}: ${DYNAMIC_RUN_ERROR}")
endif()
execute_process(
    COMMAND "${READELF}" -d "${DYNAMIC_EXECUTABLE}"
    RESULT_VARIABLE DYNAMIC_ELF_RESULT OUTPUT_VARIABLE DYNAMIC_ELF_OUTPUT)
if(NOT DYNAMIC_ELF_RESULT EQUAL 0 OR
   NOT DYNAMIC_ELF_OUTPUT MATCHES "libocelotlrt.so.1")
    message(FATAL_ERROR "dynamic executable has no libocelotlrt.so.1 dependency")
endif()

execute_process(
    COMMAND "${LINKER}" "${OBJECT}" "${RUNTIME_STATIC}" -o "${STATIC_EXECUTABLE}"
    RESULT_VARIABLE STATIC_LINK_RESULT ERROR_VARIABLE STATIC_LINK_ERROR)
if(NOT STATIC_LINK_RESULT EQUAL 0)
    message(FATAL_ERROR "static runtime link failed: ${STATIC_LINK_ERROR}")
endif()
execute_process(
    COMMAND "${STATIC_EXECUTABLE}"
    RESULT_VARIABLE STATIC_RUN_RESULT ERROR_VARIABLE STATIC_RUN_ERROR)
if(NOT STATIC_RUN_RESULT EQUAL 7)
    message(FATAL_ERROR
        "static runtime execution returned ${STATIC_RUN_RESULT}: ${STATIC_RUN_ERROR}")
endif()
execute_process(
    COMMAND "${READELF}" -d "${STATIC_EXECUTABLE}"
    RESULT_VARIABLE STATIC_ELF_RESULT OUTPUT_VARIABLE STATIC_ELF_OUTPUT)
if(NOT STATIC_ELF_RESULT EQUAL 0 OR STATIC_ELF_OUTPUT MATCHES "libocelotlrt")
    message(FATAL_ERROR "static executable unexpectedly depends on libocelotlrt")
endif()

execute_process(
    COMMAND "${NM}" -D --defined-only "${RUNTIME_SHARED}"
    RESULT_VARIABLE NM_SHARED_RESULT OUTPUT_VARIABLE NM_SHARED_OUTPUT
    ERROR_VARIABLE NM_SHARED_ERROR)
if(NOT NM_SHARED_RESULT EQUAL 0)
    message(FATAL_ERROR "shared runtime symbol inspection failed: ${NM_SHARED_ERROR}")
endif()
foreach(SYMBOL ocelotl_rt_v1_alloc ocelotl_rt_v1_free ocelotl_rt_v1_last_error)
    if(NOT NM_SHARED_OUTPUT MATCHES "${SYMBOL}")
        message(FATAL_ERROR "shared runtime does not export ${SYMBOL}")
    endif()
endforeach()
string(REGEX MATCHALL "[^\n]+" EXPORTED_LINES "${NM_SHARED_OUTPUT}")
list(LENGTH EXPORTED_LINES EXPORTED_LINE_COUNT)
if(NOT EXPORTED_LINE_COUNT EQUAL 3)
    message(FATAL_ERROR
        "shared runtime exports symbols outside the v1 ABI:\n${NM_SHARED_OUTPUT}")
endif()
