cmake_minimum_required(VERSION 3.18)

# ======================================================================
#               Test Application Configuration
# ======================================================================
set(DMOD_MODULE_NAME        test_dmclk)
set(DMOD_MODULE_VERSION     "0.1" CACHE STRING "DMOD module version")
set(DMOD_AUTHOR_NAME        "DMCLK Test")
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
    DESCRIPTION "DMCLK Test Application"
    LANGUAGES C CXX)
    
# ======================================================================
#               Import dmod functions and macros
# ======================================================================
set(DMOD_DIR ${dmod_SOURCE_DIR} CACHE PATH "DMOD source directory")
set(DMOD_SCRIPTS_DIR ${DMOD_DIR}/scripts CACHE PATH "DMOD scripts directory")
include(${DMOD_DIR}/paths.cmake)
dmod_setup_external_module()

# ======================================================================
#               Create test application module
# ======================================================================
dmod_add_library(${DMOD_MODULE_NAME} ${DMOD_MODULE_VERSION}
    test_dmclk.c
)

# Mark as application module
set_target_properties(${DMOD_MODULE_NAME} PROPERTIES DMOD_MODULE_TYPE "Dmod_ModuleType_Application")

# Link with required modules
dmod_link_modules(${DMOD_MODULE_NAME}
    dmdrvi
    dmini
)

# Add dmclk headers manually since it's not in the registry
target_include_directories(${DMOD_MODULE_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)
