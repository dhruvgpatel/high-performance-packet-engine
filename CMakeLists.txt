cmake_minimum_required(VERSION 3.14)
project(PacketProcessingEngine
    VERSION 1.0.0
    DESCRIPTION "High-Performance Multithreaded Network Packet Processing Engine in Modern C++17"
    LANGUAGES CXX
)

# Enforce C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for language servers
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build Options
option(BUILD_TESTS "Build GoogleTest unit and integration tests" ON)
option(ENABLE_ASAN "Enable AddressSanitizer (ASan)" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer (TSan)" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer (UBSan)" OFF)

# Compiler Warning Flags
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wconversion
        -Wsign-conversion
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
    )
endif()

# Sanitizers configuration
if(ENABLE_ASAN)
    message(STATUS "Enabling AddressSanitizer (ASan)")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address)
endif()

if(ENABLE_TSAN)
    if(ENABLE_ASAN)
        message(FATAL_ERROR "AddressSanitizer and ThreadSanitizer cannot be used together.")
    endif()
    message(STATUS "Enabling ThreadSanitizer (TSan)")
    add_compile_options(-fsanitize=thread -g)
    add_link_options(-fsanitize=thread)
endif()

if(ENABLE_UBSAN)
    message(STATUS "Enabling UndefinedBehaviorSanitizer (UBSan)")
    add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=undefined)
endif()

# Threading support
find_package(Threads REQUIRED)

# Core Library Sources
set(CORE_SOURCES
    src/packet.cpp
    src/socket_utils.cpp
    src/packet_receiver.cpp
    src/udp_receiver.cpp
    src/tcp_receiver.cpp
    src/worker_thread_pool.cpp
    src/packet_validator.cpp
    src/packet_classifier.cpp
    src/packet_processor.cpp
    src/metrics_collector.cpp
    src/logger.cpp
    src/web_dashboard_server.cpp
)

# Core Static Library
add_library(packet_engine_core STATIC ${CORE_SOURCES})
target_include_directories(packet_engine_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(packet_engine_core PUBLIC Threads::Threads)

# Main Server Executable
add_executable(packet_engine src/main.cpp)
target_link_libraries(packet_engine PRIVATE packet_engine_core)

# Packet Generator Tool Executable
add_executable(packet_generator tools/packet_generator.cpp)
target_link_libraries(packet_generator PRIVATE packet_engine_core)

# Testing
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
