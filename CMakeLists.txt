cmake_minimum_required(VERSION 3.16)

project(RxRevoltChain
  VERSION 0.1
  DESCRIPTION "RxRevoltChain: A custom blockchain rewarding IPFS pinning for healthcare cost data"
  LANGUAGES CXX
)

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
# main.cpp is expected to be in src/ (or at top level if desired).
set(MAIN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
if(NOT EXISTS "${MAIN_SOURCE}")
  # Fall back if main.cpp is located at top-level or user placed it elsewhere
  set(MAIN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
endif()

add_executable(rxrevoltchain
  ${MAIN_SOURCE}
  ${RXREVOLTCHAIN_SOURCES}
)

# ------------------------------------------------------------------------------
# Link libraries to the node
# ------------------------------------------------------------------------------
target_link_libraries(rxrevoltchain
  PRIVATE
  OpenSSL::SSL
  OpenSSL::Crypto
  Threads::Threads
  ZLIB::ZLIB
)

# ------------------------------------------------------------------------------
# Installation (optional)
# ------------------------------------------------------------------------------
install(TARGETS rxrevoltchain
  RUNTIME DESTINATION bin
)

# ------------------------------------------------------------------------------
# Testing (optional)
# ------------------------------------------------------------------------------
option(BUILD_TESTS "Build the test suite" OFF)
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()

message(STATUS "========== RxRevoltChain Configuration ==========")
message(STATUS "Project Name:        ${PROJECT_NAME}")
message(STATUS "Project Version:     ${PROJECT_VERSION}")
message(STATUS "OpenSSL Version:     ${OpenSSL_VERSION}")
message(STATUS "C++ Standard:        ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Tests:         ${BUILD_TESTS}")
message(STATUS "=================================================")
