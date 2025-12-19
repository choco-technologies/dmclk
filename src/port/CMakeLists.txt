
# ======================================================================
#               Parameters
# ======================================================================
set(DMCLK_MCU_SERIES "stm32f7" CACHE STRING "Target MCU series")

# ======================================================================
#               dmclk Module Configuration
# ======================================================================
# Name of the module 
set(DMOD_MODULE_NAME        dmclk_port)

# Version is already set above and used in project()
# No need to set it again here

# Author (should be string)
set(DMOD_AUTHOR_NAME        "Patryk Kubiak")

# Stack size for the module (should be integer)
set(DMOD_STACK_SIZE         1024)

# Determine common source files based on MCU family
set(COMMON_SOURCES "")
if(DMCLK_MCU_SERIES MATCHES "^stm32")
    # Add STM32 common implementation for all STM32 families
    set(COMMON_SOURCES stm32_common/stm32_common.c)
endif()

#
#   dmod_add_library - create a library module
#   it has the same signature as add_library
#   and can be used in the same way after the creation
#   (for example, to link libraries)
#
dmod_add_library(${DMOD_MODULE_NAME} ${DMOD_MODULE_VERSION}
    # List of source files - can include C and C++ files
    ${DMCLK_MCU_SERIES}/port.c
    ${COMMON_SOURCES}
)

target_include_directories(${DMOD_MODULE_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)