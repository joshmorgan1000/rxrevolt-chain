cmake_minimum_required(VERSION 3.16)

project(rxrevolt-chain
  VERSION 0.1
  DESCRIPTION "RxRevoltChain: A custom blockchain rewarding IPFS pinning for healthcare cost data"
  LANGUAGES CXX
)

# ------------------------------------------------------------------------------
# Global settings
# ------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
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

# ------------------------------------------------------------------------------
# Gather sources
# ------------------------------------------------------------------------------
# Since the project is mostly header-only, we only expect a few .cpp files (like main.cpp).
# We can still collect any optional .cpp implementations if they appear in the future.
file(GLOB_RECURSE RXREVOLTCHAIN_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

# ------------------------------------------------------------------------------
# Create main executable
# ------------------------------------------------------------------------------
set(MAIN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
if(NOT EXISTS "${MAIN_SOURCE}")
  set(MAIN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/consensus
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/network
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ipfs_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/src/pinner
    ${CMAKE_CURRENT_SOURCE_DIR}/src/util
    ${CURL_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
    ${SQLite3_INCLUDE_DIRS}
)

add_executable(rxrevolt-chain
    ${MAIN_SOURCE}
)

# ------------------------------------------------------------------------------
# Link libraries to the node
# ------------------------------------------------------------------------------
target_link_libraries(rxrevolt-chain
    PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    ZLIB::ZLIB
    CURL::libcurl
    SQLite::SQLite3
)

# ------------------------------------------------------------------------------
# Installation (optional)
# ------------------------------------------------------------------------------
install(TARGETS rxrevolt-chain
    RUNTIME DESTINATION bin
)

# ------------------------------------------------------------------------------
# Testing (optional)
# ------------------------------------------------------------------------------
# Turn BUILD_TESTS to ON to enable testing
set(BUILD_TESTS ON CACHE BOOL "Build tests" FORCE)

if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()

message(STATUS "========== rxrevolt-chain Configuration ==========")
message(STATUS "Project Name:        ${PROJECT_NAME}")
message(STATUS "Project Version:     ${PROJECT_VERSION}")
message(STATUS "OpenSSL Version:     ${OPENSSL_VERSION}")
message(STATUS "C++ Standard:        ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Tests:         ${BUILD_TESTS}")
message(STATUS "==================================================")
