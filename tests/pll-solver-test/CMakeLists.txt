set(DMOD_MODULE_NAME        pll_solver_test)
set(DMOD_MODULE_VERSION     ${PROJECT_VERSION})
set(DMOD_AUTHOR_NAME        "Patryk Kubiak")
set(DMOD_STACK_SIZE         1024)
set(DMOD_PRIORITY           0)

# stm32_common.c is pure PLL arithmetic and register-field packing/unpacking
# with no actual hardware access beyond what a stack-allocated fake
# RCC_TypeDef gives it - compiled straight into this test rather than going
# through a real (target-only) dmclk_port build.
dmod_add_test(${DMOD_MODULE_NAME} ${DMOD_MODULE_VERSION}
    pll_solver_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/port/stm32_common/stm32_common.c
)

target_include_directories(${DMOD_MODULE_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/port/stm32_common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# dmclk_port_if pulls in the generated dmclk_port_defs.h that dmclk_port.h
# (included transitively via stm32_common.h) needs, without requiring the
# real (target-only) dmclk_port module to be built.
target_link_libraries(${DMOD_MODULE_NAME} dmclk_port_if)

# Register the test module with ctest - install the test module's
# dependencies with dmf-get, then execute it through dmod_loader.
find_program(DMF_GET_EXECUTABLE dmf-get HINTS ${DMOD_TOOLS_BIN_DIR} PATHS ENV PATH)
find_program(DMOD_LOADER_EXECUTABLE dmod_loader HINTS ${DMOD_TOOLS_BIN_DIR} PATHS ENV PATH)

if(DMF_GET_EXECUTABLE AND DMOD_LOADER_EXECUTABLE)
    add_test(
        NAME ${DMOD_MODULE_NAME}
        COMMAND ${CMAKE_COMMAND} -E env DMOD_DMF_DIR=${DMOD_DMF_DIR}
            bash -c "${DMOD_LOADER_EXECUTABLE} ${DMOD_DMF_DIR}/${DMOD_MODULE_NAME}.dmf"
    )
else()
    message(WARNING "dmf-get/dmod_loader not found - 'ctest' will not run ${DMOD_MODULE_NAME}. Build the tools first with -DDMOD_BUILD_TOOLS=ON")
endif()
