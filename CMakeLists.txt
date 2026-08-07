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
    src/frontend/Token.cpp
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
