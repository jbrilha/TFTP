cmake_minimum_required(VERSION 3.25)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_COMPILER "gcc")

add_compile_options(-Wall -Wextra -Wpedantic)

project(TFTP LANGUAGES C CXX VERSION 1.0.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/build/debug)

# add_subdirectory(include)
include_directories(${CMAKE_SOURCE_DIR}/include)

add_library(shared 
    src/common.c
    src/utils.c
    src/tftp.c
)

add_executable(server src/tftp_server.c)
target_link_libraries(server shared)

add_executable(client src/tftp_client.c)
target_link_libraries(client shared)
