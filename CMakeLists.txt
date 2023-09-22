cmake_minimum_required(VERSION 3.10)
project(FALSE_COMPILER)

include(CheckIPOSupported)
check_ipo_supported(RESULT supported OUTPUT error)

add_executable(false-c-port src/false-port.c src/compile.c src/util.c src/cmdline.c src/disassemble.c)
target_include_directories(false-c-port PRIVATE ${CMAKE_SOURCE_DIR}/src/)
target_compile_features(false-c-port PRIVATE c_std_23)
target_compile_options(false-c-port PRIVATE -march=native -Wall -Wextra -Wpedantic)
target_link_libraries(false-c-port PRIVATE capstone)

if( supported )
    message(STATUS "IPO / LTO enabled")
    set_target_properties(false-c-port PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(STATUS "IPO / LTO not supported: <${error}>")
endif()