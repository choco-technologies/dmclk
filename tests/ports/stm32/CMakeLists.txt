cmake_minimum_required(VERSION 3.18)
project(dmclk_stm32_port_tests C)
enable_testing()

# The production port owns its source list. Build it as a native library;
# the fixture supplies register memory and the SAL clock/critical sections.
add_subdirectory(../../../src/port/stm32_common stm32_common)
add_library(dmclk_port_native STATIC clock_fixture.c)
dmclk_add_stm32_sources(dmclk_port_native)
target_include_directories(dmclk_port_native PUBLIC stubs ../../../include)
target_compile_options(dmclk_port_native PRIVATE -Wall -Wextra -Werror -UNDEBUG)

add_executable(sai_clock_test sai_clock_test.c)
target_link_libraries(sai_clock_test PRIVATE dmclk_port_native)
target_compile_options(sai_clock_test PRIVATE -Wall -Wextra -Werror -UNDEBUG)
foreach(case rates ownership invalid source overflow lock_timeout stop_timeout rollback concurrency guards unsupported)
    add_test(NAME sai_${case} COMMAND sai_clock_test ${case})
endforeach()
