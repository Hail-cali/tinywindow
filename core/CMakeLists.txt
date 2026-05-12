cmake_minimum_required(VERSION 3.20)
project(nanofilter C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Compiler flags
add_compile_options(-Wall -Wextra -Wpedantic -O2)

# Threading
find_package(Threads REQUIRED)

# Core library
add_library(nanofilter SHARED
    src/hash.c
    src/timing_cms.c
    src/sliding_bf.c
    src/window.c
    src/memory.c
)
target_include_directories(nanofilter PUBLIC include)
target_link_libraries(nanofilter PRIVATE Threads::Threads m)

# Static library (for embedding)
add_library(nanofilter_static STATIC
    src/hash.c
    src/timing_cms.c
    src/sliding_bf.c
    src/window.c
    src/memory.c
)
target_include_directories(nanofilter_static PUBLIC include)
target_link_libraries(nanofilter_static PRIVATE Threads::Threads m)
set_target_properties(nanofilter_static PROPERTIES OUTPUT_NAME nanofilter)

# Tests
enable_testing()

add_executable(test_hash test/test_hash.c)
target_link_libraries(test_hash nanofilter_static Threads::Threads m)
add_test(NAME test_hash COMMAND test_hash)

add_executable(test_timing_cms test/test_timing_cms.c)
target_link_libraries(test_timing_cms nanofilter_static Threads::Threads m)
add_test(NAME test_timing_cms COMMAND test_timing_cms)

add_executable(test_window test/test_window.c)
target_link_libraries(test_window nanofilter_static Threads::Threads m)
add_test(NAME test_window COMMAND test_window)

add_executable(test_sliding_bf test/test_sliding_bf.c)
target_link_libraries(test_sliding_bf nanofilter_static Threads::Threads m)
add_test(NAME test_sliding_bf COMMAND test_sliding_bf)

# Benchmark
add_executable(bench_throughput test/bench_throughput.c)
target_link_libraries(bench_throughput nanofilter_static Threads::Threads m)

# Install
install(TARGETS nanofilter
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(FILES include/nanofilter.h DESTINATION include)
