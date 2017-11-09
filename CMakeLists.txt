cmake_minimum_required(VERSION 2.8)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include_directories(include)

option(RNETLIB_USE_RDMA "Use RDMA" OFF)

# output path for runtime programs
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

set(RNETLIB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/rnetlib")

add_subdirectory(examples)
