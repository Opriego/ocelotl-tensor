cmake_minimum_required(VERSION 3.22)

project(
    ocelotl_tensor
    VERSION 0.1.0
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(CTest)
include(GNUInstallDirs)

#
# Stable C runtime ABI. Both variants deliberately share the link name:
# libocelotlrt.so for dynamic consumers and libocelotlrt.a for static ones.
#

add_library(ocelotlrt SHARED runtime/runtime.c)
add_library(ocelotlrt_static STATIC runtime/runtime.c)

foreach(RUNTIME_TARGET ocelotlrt ocelotlrt_static)
    target_include_directories(
        ${RUNTIME_TARGET}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
    target_compile_options(
        ${RUNTIME_TARGET}
        PRIVATE -Wall -Wextra -Wpedantic
    )
    set_target_properties(
        ${RUNTIME_TARGET}
        PROPERTIES
            C_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN YES
            OUTPUT_NAME ocelotlrt
    )
endforeach()

target_compile_definitions(
    ocelotlrt
    PRIVATE OCELOTLRT_BUILDING OCELOTLRT_SHARED
    INTERFACE OCELOTLRT_SHARED
)
set_target_properties(
    ocelotlrt
    PROPERTIES
        VERSION 1.0.0
        SOVERSION 1
)

#
# LLVM
#

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

separate_arguments(
    LLVM_DEFINITIONS_LIST
    NATIVE_COMMAND
    "${LLVM_DEFINITIONS}"
)

llvm_map_components_to_libnames(
    LLVM_LIBS
    Core
    Support

    # TargetMachine and its target-independent emission pipeline.
    Target
    MC
    CodeGen

    # LLVM New Pass Manager and standard optimization pipelines.
    Passes

    # Assembly printers and every target backend built into LLVM. This keeps
    # --target useful for cross-compilation without hard-coding a host ISA.
    AsmPrinter
    AllTargetsCodeGens
)

#
# Ocelotl compiler library
#

add_library(
    ocelotl_compiler

    src/frontend/Token.cpp
    src/frontend/Lexer.cpp
    src/frontend/Parser.cpp

    src/semantic/SymbolTable.cpp
    src/semantic/SemanticAnalyzer.cpp

    src/ir/IRGenerator.cpp
    src/ir/IR.cpp
    src/ir/IRVerifier.cpp

    src/codegen/llvm/LLVMCodeGenerator.cpp
    src/codegen/llvm/LLVMOptimizer.cpp
    src/codegen/llvm/TargetCodeGenerator.cpp
)

#
# Ocelotl public headers.
#
# LLVM headers are also PUBLIC because LLVM types such as llvm::Module
# are exposed by Ocelotl's public code-generation interface.
#
# LLVM is marked SYSTEM so warnings originating from LLVM headers are
# not treated as warnings from the Ocelotl source tree.
#

target_include_directories(
    ocelotl_compiler
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(
    ocelotl_compiler
    SYSTEM PUBLIC
        ${LLVM_INCLUDE_DIRS}
)

#
# LLVM compile definitions must propagate to targets consuming
# ocelotl_compiler because those consumers may compile headers
# containing LLVM declarations.
#

target_compile_definitions(
    ocelotl_compiler
    PUBLIC
        ${LLVM_DEFINITIONS_LIST}
)

target_compile_options(
    ocelotl_compiler
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

#
# LLVM libraries are part of the compiler library's public link
# interface because consumers of ocelotl_compiler may use LLVM types
# exposed by its public API.
#

target_link_libraries(
    ocelotl_compiler
    PUBLIC
        ${LLVM_LIBS}
)

#
# Command-line compiler
#

add_executable(
    ocelotlc
    src/main.cpp
)

target_link_libraries(
    ocelotlc
    PRIVATE
        ocelotl_compiler
)

target_compile_options(
    ocelotlc
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

#
# Installation
#

install(
    TARGETS ocelotlc ocelotlrt ocelotlrt_static
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(
    DIRECTORY include/ocelotl/runtime
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ocelotl
)

install(
    FILES debian/ocelotlc.1
    DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
)

#
# Tests
#

if(BUILD_TESTING)

    find_package(GTest REQUIRED)

    # Used only to validate the format and architecture of emitted objects.
    llvm_map_components_to_libnames(
        LLVM_TEST_LIBS
        Object
    )

    add_executable(
        ocelotl_tests

        tests/frontend/LexerTests.cpp
        tests/frontend/ParserTests.cpp

        tests/ast/ASTTests.cpp

        tests/semantic/SemanticAnalyzerTests.cpp

        tests/ir/IRGeneratorTests.cpp
        tests/ir/IRVerifierTests.cpp

        tests/codegen/LLVMCodeGeneratorTests.cpp
        tests/codegen/LLVMOptimizerTests.cpp
        tests/codegen/TargetCodeGeneratorTests.cpp
        tests/runtime/RuntimeTests.cpp
    )

    target_link_libraries(
        ocelotl_tests
        PRIVATE
            ocelotl_compiler
            ocelotlrt_static
            ${LLVM_TEST_LIBS}
            GTest::gtest_main
    )

    target_compile_options(
        ocelotl_tests
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

    include(GoogleTest)

    gtest_discover_tests(
        ocelotl_tests
    )

    find_program(LLVM_NM_EXECUTABLE NAMES llvm-nm nm)
    find_program(LLVM_READELF_EXECUTABLE NAMES llvm-readelf readelf)

    if(UNIX AND LLVM_NM_EXECUTABLE AND LLVM_READELF_EXECUTABLE)
        add_test(
            NAME RuntimeABI.GeneratedCallsAndLinking
            COMMAND
                ${CMAKE_COMMAND}
                -DCOMPILER=$<TARGET_FILE:ocelotlc>
                -DLINKER=${CMAKE_CXX_COMPILER}
                -DRUNTIME_SHARED=$<TARGET_FILE:ocelotlrt>
                -DRUNTIME_STATIC=$<TARGET_FILE:ocelotlrt_static>
                -DRUNTIME_DIR=$<TARGET_FILE_DIR:ocelotlrt>
                -DNM=${LLVM_NM_EXECUTABLE}
                -DREADELF=${LLVM_READELF_EXECUTABLE}
                -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/examples/runtime_allocation.oc
                -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/runtime-abi-test
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/runtime/RuntimeLinkTest.cmake
        )
    else()
        message(STATUS
            "nm/readelf not found or non-UNIX host: runtime ABI link test disabled"
        )
    endif()

    find_program(
        LLVM_READOBJ_EXECUTABLE
        NAMES llvm-readobj
    )

    if(LLVM_READOBJ_EXECUTABLE)
        add_test(
            NAME CrossTarget.X86AndAArch64ELF
            COMMAND
                ${CMAKE_COMMAND}
                -DCOMPILER=$<TARGET_FILE:ocelotlc>
                -DLLVM_READOBJ=${LLVM_READOBJ_EXECUTABLE}
                -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/examples/optimization.oc
                -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cross-target-tests
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/codegen/CrossTargetGenerationTest.cmake
        )
    else()
        message(STATUS
            "llvm-readobj not found: external ELF inspection test disabled"
        )
    endif()

    find_program(
        AARCH64_C_COMPILER
        NAMES aarch64-linux-gnu-gcc
    )
    find_program(
        QEMU_AARCH64_EXECUTABLE
        NAMES qemu-aarch64 qemu-aarch64-static
    )

    if(AARCH64_C_COMPILER AND QEMU_AARCH64_EXECUTABLE)
        set(AARCH64_RUNTIME_SYSROOT "")
        if(EXISTS /usr/aarch64-linux-gnu)
            set(AARCH64_RUNTIME_SYSROOT /usr/aarch64-linux-gnu)
        endif()

        add_test(
            NAME CrossTarget.AArch64LinkAndRun
            COMMAND
                ${CMAKE_COMMAND}
                -DCOMPILER=$<TARGET_FILE:ocelotlc>
                -DAARCH64_CC=${AARCH64_C_COMPILER}
                -DQEMU_AARCH64=${QEMU_AARCH64_EXECUTABLE}
                -DAARCH64_SYSROOT=${AARCH64_RUNTIME_SYSROOT}
                -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/examples/optimization.oc
                -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/aarch64-runtime-test
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/codegen/AArch64RuntimeTest.cmake
        )
    else()
        message(STATUS
            "AArch64 cross compiler and/or qemu-aarch64 not found: "
            "optional AArch64 link/execution test disabled"
        )
    endif()

    if(UNIX)
        add_test(
            NAME NativeExecution.TrueBranch
            COMMAND
                ${CMAKE_COMMAND}
                -DCOMPILER=$<TARGET_FILE:ocelotlc>
                -DLINKER=${CMAKE_CXX_COMPILER}
                -DSOURCE_VALUE=12
                -DEXPECTED=13
                -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/native-tests
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/codegen/NativeExecutionTest.cmake
        )

        add_test(
            NAME NativeExecution.FalseBranch
            COMMAND
                ${CMAKE_COMMAND}
                -DCOMPILER=$<TARGET_FILE:ocelotlc>
                -DLINKER=${CMAKE_CXX_COMPILER}
                -DSOURCE_VALUE=2
                -DEXPECTED=1
                -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/native-tests
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/codegen/NativeExecutionTest.cmake
        )

        add_test(
            NAME NativeExecution.Status42
            COMMAND
                ${CMAKE_COMMAND}
                -DCOMPILER=$<TARGET_FILE:ocelotlc>
                -DLINKER=${CMAKE_CXX_COMPILER}
                -DSOURCE_VALUE=41
                -DEXPECTED=42
                -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/native-status-42
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/codegen/NativeExecutionTest.cmake
        )

        add_test(
            NAME NativeExecution.StatusZero
            COMMAND
                ${CMAKE_COMMAND}
                -DCOMPILER=$<TARGET_FILE:ocelotlc>
                -DLINKER=${CMAKE_CXX_COMPILER}
                -DSOURCE_VALUE=1
                -DEXPECTED=0
                -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/native-status-zero
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/codegen/NativeExecutionTest.cmake
        )

        # The Ocelotl value is i64 -1 and LLVM truncates it to i32 -1. POSIX
        # process observation reports the low 8-bit status, 255.
        add_test(
            NAME NativeExecution.NegativeStatus
            COMMAND
                ${CMAKE_COMMAND}
                -DCOMPILER=$<TARGET_FILE:ocelotlc>
                -DLINKER=${CMAKE_CXX_COMPILER}
                -DSOURCE_VALUE=0
                -DEXPECTED=255
                -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/native-status-negative
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/codegen/NativeExecutionTest.cmake
        )

        foreach(OPT_LEVEL 0 1 2 3)
            add_test(
                NAME NativeOptimization.O${OPT_LEVEL}
                COMMAND
                    ${CMAKE_COMMAND}
                    -DCOMPILER=$<TARGET_FILE:ocelotlc>
                    -DLINKER=${CMAKE_CXX_COMPILER}
                    -DSOURCE_VALUE=12
                    -DEXPECTED=13
                    -DOPT_LEVEL=-O${OPT_LEVEL}
                    -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/native-opt-O${OPT_LEVEL}
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/codegen/NativeExecutionTest.cmake
            )
        endforeach()
    endif()

endif()
