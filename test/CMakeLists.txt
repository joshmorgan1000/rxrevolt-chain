#
# test/CMakeLists.txt for RxRevoltChain
# This file will discover and build the unit tests and integration tests,
# linking them against the main RxRevoltChain modules.
#

cmake_minimum_required(VERSION 3.16)

project(RxRevoltChainTests VERSION 1.0 LANGUAGES CXX)

# ------------------------------------------------------------------------------
# Global settings
# ------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Optionally increase warnings
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

# ------------------------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------------------------
# 1. Find OpenSSL (ECDSA, hashing, etc.)
find_package(OpenSSL REQUIRED)
if(NOT OpenSSL_FOUND)
  message(FATAL_ERROR "OpenSSL is required but not found.")
endif()

# 2. Find Threads (for concurrency, thread pools, etc.)
find_package(Threads REQUIRED)
if(NOT Threads_FOUND)
  message(FATAL_ERROR "Threads library required but not found.")
endif()

# 3. ZLIB (needed for compression)
find_package(ZLIB REQUIRED)
if(NOT ZLIB_FOUND)
  message(FATAL_ERROR "ZLIB is required but not found.")
endif()

# 4. CURL (for HTTP requests)
find_package(CURL REQUIRED)
if(NOT CURL_FOUND)
  message(FATAL_ERROR "CURL is required but not found.")
endif()

# 5. SQLite3 (for local storage)
find_package(SQLite3 REQUIRED)
if(NOT SQLite3_FOUND)
  message(FATAL_ERROR "SQLite3 is required but not found.")
endif()

# 6. GoogleTest (for unit tests)
find_package(GTest REQUIRED)
if(NOT GTest_FOUND)
  message(FATAL_ERROR "GoogleTest is required but not found.")
endif()

include_directories(
    ${CMAKE_CURRENT_LIST_DIR}/..
    ${CMAKE_CURRENT_LIST_DIR}/../config
    ${CMAKE_CURRENT_LIST_DIR}/../src
    ${CMAKE_CURRENT_LIST_DIR}/../src/consensus
    ${CMAKE_CURRENT_LIST_DIR}/../src/core
    ${CMAKE_CURRENT_LIST_DIR}/../src/network
    ${CMAKE_CURRENT_LIST_DIR}/../src/connectors
    ${CMAKE_CURRENT_LIST_DIR}/../src/ipfs_integration
    ${CMAKE_CURRENT_LIST_DIR}/../src/pinner
    ${CMAKE_CURRENT_LIST_DIR}/../src/util
    ${CMAKE_CURRENT_LIST_DIR}/integration
    ${CMAKE_CURRENT_LIST_DIR}/unit
    ${CURL_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
    ${SQLite3_INCLUDE_DIRS}
    ${GTEST_INCLUDE_DIRS}
)

# Now define our single test executable:
add_executable(rxrevoltchain_tests
    test_runner.cpp
)

target_link_libraries(rxrevoltchain_tests
    PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
    Threads::Threads
    CURL::libcurl
    SQLite::SQLite3
    GTest::GTest
)

# Force the linker language to CXX, ensuring no "Cannot determine link language" error:
set_target_properties(rxrevoltchain_tests PROPERTIES LINKER_LANGUAGE CXX)

# Provide includes for the source directories
target_include_directories(rxrevoltchain_tests
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../src
    ${CMAKE_CURRENT_LIST_DIR}/..
)

# Enable CTest
enable_testing()
add_test(NAME rxrevoltchain_tests COMMAND rxrevoltchain_tests)

message(STATUS "Configured test/CMakeLists.txt for both unit and integration tests (with includes in test_runner, using C++ linker).")
