cmake_minimum_required(VERSION 3.16)
project(SmartCompress VERSION 1.0 LANGUAGES CXX)

# C++20 Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler Optimization Flags
# Release build with maximum optimizations
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")

# Additional optimization flags for specific compilers
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto -ffast-math")
endif()

# Debug build with debug symbols
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")

# Default to Release build if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Source files
set(SOURCES
    main.cpp
)

# Header files
set(HEADERS
    compression_engine.hpp
)

# Create executable
add_executable(compress_engine ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(compress_engine PRIVATE ${CMAKE_SOURCE_DIR})

# Compiler-specific warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(compress_engine PRIVATE 
        -Wall 
        -Wextra 
        -Wpedantic
        -Wconversion
        -Wsign-conversion
    )
endif()

# Installation rules
install(TARGETS compress_engine DESTINATION bin)

# Print configuration summary
message(STATUS "")
message(STATUS "SmartCompress Configuration Summary")
message(STATUS "====================================")
message(STATUS "Build Type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard:    C++${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler:        ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Install Prefix:  ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
