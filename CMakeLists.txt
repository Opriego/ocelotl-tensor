cmake_minimum_required(VERSION 3.22)

project(
    ocelotl_tensor
    VERSION 0.1.0
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
include(GNUInstallDirs)

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
    TARGETS ocelotlc
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
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
    )

    target_link_libraries(
        ocelotl_tests
        PRIVATE
            ocelotl_compiler
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
