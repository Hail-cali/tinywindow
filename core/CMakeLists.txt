cmake_minimum_required(VERSION 3.20)
project(tinywindow C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Compiler flags
add_compile_options(-Wall -Wextra -Wpedantic -O2)

# Threading
find_package(Threads REQUIRED)

# Core library
add_library(tinywindow SHARED
    src/hash.c
    src/timing_cms.c
    src/sliding_bf.c
    src/window.c
    src/memory.c
)
target_include_directories(tinywindow PUBLIC include)
target_link_libraries(tinywindow PRIVATE Threads::Threads m)

# Static library (for embedding)
add_library(tinywindow_static STATIC
    src/hash.c
    src/timing_cms.c
    src/sliding_bf.c
    src/window.c
    src/memory.c
)
target_include_directories(tinywindow_static PUBLIC include)
target_link_libraries(tinywindow_static PRIVATE Threads::Threads m)
set_target_properties(tinywindow_static PROPERTIES OUTPUT_NAME tinywindow)

# Tests
enable_testing()

add_executable(test_hash test/test_hash.c)
target_link_libraries(test_hash tinywindow_static Threads::Threads m)
add_test(NAME test_hash COMMAND test_hash)

add_executable(test_timing_cms test/test_timing_cms.c)
target_link_libraries(test_timing_cms tinywindow_static Threads::Threads m)
add_test(NAME test_timing_cms COMMAND test_timing_cms)

add_executable(test_window test/test_window.c)
target_link_libraries(test_window tinywindow_static Threads::Threads m)
add_test(NAME test_window COMMAND test_window)

add_executable(test_sliding_bf test/test_sliding_bf.c)
target_link_libraries(test_sliding_bf tinywindow_static Threads::Threads m)
add_test(NAME test_sliding_bf COMMAND test_sliding_bf)

# Benchmark
add_executable(bench_throughput test/bench_throughput.c)
target_link_libraries(bench_throughput tinywindow_static Threads::Threads m)

# Install
install(TARGETS tinywindow
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(FILES include/tinywindow.h DESTINATION include)
