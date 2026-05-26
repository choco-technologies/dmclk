cmake_minimum_required(VERSION 3.18)

# ======================================================================
#               dmclk_test Module Configuration
# ======================================================================
set(DMOD_MODULE_NAME        dmclk_test)
set(DMOD_AUTHOR_NAME        "Patryk Kubiak")
set(DMOD_STACK_SIZE         2048)

# ======================================================================
#               Create dmclk_test application module
# ======================================================================
dmod_add_executable(${DMOD_MODULE_NAME} ${DMOD_MODULE_VERSION}
    dmclk_test.c
)

target_link_libraries(${DMOD_MODULE_NAME}
    dmclk_port_if
)
