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

    src/codegen/llvm/LLVMCodeGenerator.cpp
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

#
# Tests
#

if(BUILD_TESTING)

    find_package(GTest REQUIRED)

    add_executable(
        ocelotl_tests

        tests/frontend/LexerTests.cpp
        tests/frontend/ParserTests.cpp

        tests/ast/ASTTests.cpp

        tests/semantic/SemanticAnalyzerTests.cpp

        tests/ir/IRGeneratorTests.cpp

        tests/codegen/LLVMCodeGeneratorTests.cpp
    )

    target_link_libraries(
        ocelotl_tests
        PRIVATE
            ocelotl_compiler
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

endif()
