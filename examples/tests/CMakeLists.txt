# Create test executable
add_executable(tests test_spsc.cpp test_lfs.cpp test_mpsc_queue.cpp)

# Link with Google Test and your library
target_link_libraries(tests
        PRIVATE
        atomic
        GTest::gtest_main
        SPSCRingBuffer
        LockFreeStack
        MPSCQueue
)

# Add test
gtest_discover_tests(tests)