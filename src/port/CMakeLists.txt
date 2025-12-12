
# ======================================================================
#               Parameters
# ======================================================================
set(DMCLK_MCU_SERIES "stm32f7" CACHE STRING "Target MCU series")

# ======================================================================
#               Architecture Compatibility Check
# ======================================================================
# Define compatible CPU architectures for each MCU family
# This ensures we don't build incompatible combinations (e.g., STM32F7 on Cortex-M4)

# Get the CPU architecture from DMOD if available
if(DEFINED DMOD_CPU)
    set(TARGET_CPU "${DMOD_CPU}")
elseif(DEFINED DMOD_ARCH)
    # Extract CPU from architecture string (e.g., "armv7-cortex-m4" -> "cortex-m4")
    string(REGEX MATCH "cortex-m[0-9]+" TARGET_CPU "${DMOD_ARCH}")
endif()

# Define CPU family to compatible core mapping
set(CPU_FAMILY_COMPAT_stm32f0 "cortex-m0+")
set(CPU_FAMILY_COMPAT_stm32f1 "cortex-m3")
set(CPU_FAMILY_COMPAT_stm32f2 "cortex-m3")
set(CPU_FAMILY_COMPAT_stm32f3 "cortex-m4")
set(CPU_FAMILY_COMPAT_stm32f4 "cortex-m4")
set(CPU_FAMILY_COMPAT_stm32f7 "cortex-m7")
set(CPU_FAMILY_COMPAT_stm32g0 "cortex-m0+")
set(CPU_FAMILY_COMPAT_stm32g4 "cortex-m4")
set(CPU_FAMILY_COMPAT_stm32h7 "cortex-m7")
set(CPU_FAMILY_COMPAT_stm32l0 "cortex-m0+")
set(CPU_FAMILY_COMPAT_stm32l1 "cortex-m3")
set(CPU_FAMILY_COMPAT_stm32l4 "cortex-m4")
set(CPU_FAMILY_COMPAT_stm32l5 "cortex-m33")
set(CPU_FAMILY_COMPAT_stm32u5 "cortex-m33")
set(CPU_FAMILY_COMPAT_stm32wb "cortex-m4")
set(CPU_FAMILY_COMPAT_stm32wl "cortex-m4")

# Check compatibility if TARGET_CPU is defined
if(DEFINED TARGET_CPU AND TARGET_CPU)
    set(EXPECTED_CPU "${CPU_FAMILY_COMPAT_${DMCLK_MCU_SERIES}}")
    if(EXPECTED_CPU)
        if(NOT TARGET_CPU STREQUAL EXPECTED_CPU)
            message(FATAL_ERROR 
                "CPU family '${DMCLK_MCU_SERIES}' is not compatible with architecture '${TARGET_CPU}'. "
                "Expected '${EXPECTED_CPU}'. "
                "Please use the correct architecture for this CPU family.")
        else()
            message(STATUS "CPU family '${DMCLK_MCU_SERIES}' is compatible with '${TARGET_CPU}'")
        endif()
    else()
        message(WARNING "No compatibility information for CPU family '${DMCLK_MCU_SERIES}'")
    endif()
endif()

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