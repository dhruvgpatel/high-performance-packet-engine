cmake_minimum_required(VERSION 3.14)

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
# For Windows/MSVC runtime compatibility
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

# Unit & Integration Test Executable
add_executable(unit_tests
    test_packet.cpp
    test_thread_safe_queue.cpp
    test_packet_validator.cpp
    test_packet_classifier.cpp
    test_packet_processor.cpp
    test_metrics.cpp
    test_worker_pool.cpp
    test_integration.cpp
)

target_link_libraries(unit_tests
    PRIVATE
    packet_engine_core
    GTest::gtest_main
    Threads::Threads
)

gtest_discover_tests(unit_tests)
