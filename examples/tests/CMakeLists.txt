# Create test executable
add_executable(tests test_spsc.cpp test_lfs.cpp test_mpsc_queue.cpp)

# Link with Google Test and your library
target_link_libraries(tests
        PRIVATE
        GTest::gtest_main
        SPSCRingBuffer
        LockFreeStack
        MPSCQueue
)
# libatomic is only needed on Linux
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(tests PRIVATE atomic)
endif()

# Add test
gtest_discover_tests(tests)