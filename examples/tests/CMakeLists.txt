# Create test executable
add_executable(SPSCRingBufferTests test_spsc.cpp)

# Link with Google Test and your library
target_link_libraries(SPSCRingBufferTests
        PRIVATE
        GTest::gtest_main
        SPSCRingBuffer
)

# Add test
gtest_discover_tests(SPSCRingBufferTests)