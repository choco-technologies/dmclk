cmake_minimum_required(VERSION 3.18)

# ======================================================================
#               dmclk_test Module Configuration
# ======================================================================
set(DMOD_MODULE_NAME        dmclk_test)
set(DMOD_MODULE_VERSION     "0.1" CACHE STRING "DMOD module version")
set(DMOD_AUTHOR_NAME        "Patryk Kubiak")
set(DMOD_STACK_SIZE         2048)

# ======================================================================
#               Fetch DMOD repository
# ======================================================================
include(FetchContent)
FetchContent_Declare(
    dmod
    GIT_REPOSITORY https://github.com/choco-technologies/dmod.git
    GIT_TAG        develop
)

# ======================================================================
#               DMOD Configuration
# ======================================================================
set(DMOD_MODE "DMOD_MODULE" CACHE STRING "DMOD build mode")
set(DMOD_BUILD_TESTS OFF CACHE BOOL "Build tests")
set(DMOD_BUILD_EXAMPLES OFF CACHE BOOL "Build examples")
set(DMOD_BUILD_TOOLS OFF CACHE BOOL "Build tools")
set(DMOD_BUILD_TEMPLATES OFF CACHE BOOL "Build templates")

FetchContent_MakeAvailable(dmod)

project(${DMOD_MODULE_NAME}
    VERSION ${DMOD_MODULE_VERSION}
    DESCRIPTION "DMCLK Clock Frequency Measurement Test Application"
    LANGUAGES C CXX)

# ======================================================================
#               Import dmod functions and macros
# ======================================================================
set(DMOD_DIR ${dmod_SOURCE_DIR} CACHE PATH "DMOD source directory")
set(DMOD_SCRIPTS_DIR ${DMOD_DIR}/scripts CACHE PATH "DMOD scripts directory")
include(${DMOD_DIR}/paths.cmake)
dmod_setup_external_module()

# ======================================================================
#               Create dmclk_test application module
# ======================================================================
dmod_add_library(${DMOD_MODULE_NAME} ${DMOD_MODULE_VERSION}
    dmclk_test.c
)

# Mark as application module
set_target_properties(${DMOD_MODULE_NAME} PROPERTIES DMOD_MODULE_TYPE "Dmod_ModuleType_Application")

# Link with dmclk_port (provides dmclk_port_delay and dmclk_port_get_current_frequency)
dmod_link_modules(${DMOD_MODULE_NAME}
    dmclk_port
)

# Add dmclk_port headers
target_include_directories(${DMOD_MODULE_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)
