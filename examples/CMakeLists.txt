cmake_minimum_required(VERSION 3.14)
project(SPSCRingBufferProject VERSION 1.0.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add Google Test
include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
)

# Fetch GoogleTest
FetchContent_GetProperties(googletest)
if(NOT googletest_POPULATED)
    FetchContent_Populate(googletest)
    add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})
endif()

# IMPORTANT: This makes gtest_discover_tests available
include(GoogleTest)

# Enable testing
enable_testing()

# Add your library
add_library(SPSCRingBuffer INTERFACE)
target_include_directories(SPSCRingBuffer INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_sources(SPSCRingBuffer INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/SPSCRingBuffer.h)

# Add tests subdirectory
add_subdirectory(tests)