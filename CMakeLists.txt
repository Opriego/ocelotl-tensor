cmake_minimum_required(VERSION 3.22)

project(
    ocelotl_tensor
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(
    ocelotl_compiler

    # Frontend
    src/frontend/Token.cpp
    src/frontend/Lexer.cpp
    src/frontend/Parser.cpp

    # include
    include/ocelotl/frontend/Token.hpp
    include/ocelotl/frontend/Lexer.hpp
    include/ocelotl/frontend/Parser.hpp

    # semantic
    src/semantic/SymbolTable.cpp
    src/semantic/SemanticAnalyzer.cpp

    # IR
    src/ir/IRGenerator.cpp

    # AST
    include/ocelotl/ast/AST.hpp
)

target_include_directories(
    ocelotl_compiler
    PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_options(
    ocelotl_compiler
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

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

include(CTest)

if(BUILD_TESTING)
    find_package(GTest REQUIRED)

    add_executable(
        ocelotl_tests

        tests/frontend/LexerTests.cpp
        tests/frontend/ParserTests.cpp        
        tests/ast/ASTTests.cpp
        tests/semantic/SemanticAnalyzerTests.cpp
        tests/ir/IRGeneratorTests.cpp
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

    gtest_discover_tests(ocelotl_tests)
endif()
