cmake_minimum_required(VERSION 2.8)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include_directories(include)

option(RNETLIB_ENABLE_OFI "Enable OFI libfabric provider" OFF)
option(RNETLIB_ENABLE_VERBS "Enable OFA Verbs provider" OFF)

# output path for runtime programs
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

set(RNETLIB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/rnetlib")

add_subdirectory(examples)
