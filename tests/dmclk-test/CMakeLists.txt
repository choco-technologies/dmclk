cmake_minimum_required(VERSION 3.18)

# ======================================================================
#               dmclk_test Module Configuration
# ======================================================================
set(DMOD_MODULE_NAME        dmclk_test)
set(DMOD_AUTHOR_NAME        "Patryk Kubiak")
set(DMOD_STACK_SIZE         2048)

# Path to the DMR file for this module
set(DMOD_DMR_PATH           ${CMAKE_SOURCE_DIR}/dmclk_test.dmr)

# ======================================================================
#               Create dmclk_test application module
# ======================================================================
dmod_add_executable(${DMOD_MODULE_NAME} ${DMOD_MODULE_VERSION}
    dmclk_test.c
)

target_link_libraries(${DMOD_MODULE_NAME}
    dmclk_port_if
)

if(DMCLK_MCU_SERIES STREQUAL "stm32f7")
    dmod_add_executable(dmclk_sai_test ${DMOD_MODULE_VERSION} sai_clock_hardware.c)
    target_link_libraries(dmclk_sai_test dmclk_port_if)
    target_include_directories(dmclk_sai_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
endif()
