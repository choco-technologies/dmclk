set(DMOD_MODULE_NAME domain_test)
set(DMOD_AUTHOR_NAME "Patryk Kubiak")
set(DMOD_STACK_SIZE 4096)
set(DMOD_PRIORITY 0)

dmod_add_test(${DMOD_MODULE_NAME} ${DMOD_MODULE_VERSION}
    domain_test.c
    ../../src/port/domain.c
    ../../src/port/stm32f7/sai_clock.c
)
target_link_libraries(${DMOD_MODULE_NAME} dmclk_port_if)
target_compile_definitions(${DMOD_MODULE_NAME} PRIVATE DMOD_dmclk_port)
target_include_directories(${DMOD_MODULE_NAME} PRIVATE ../../src/port)

find_program(DMOD_LOADER_EXECUTABLE dmod_loader HINTS ${DMOD_TOOLS_BIN_DIR})
if(DMOD_LOADER_EXECUTABLE)
    add_test(NAME ${DMOD_MODULE_NAME}
        COMMAND ${CMAKE_COMMAND} -E env DMOD_DMF_DIR=${DMOD_DMF_DIR}
            ${DMOD_LOADER_EXECUTABLE} ${DMOD_DMF_DIR}/${DMOD_MODULE_NAME}.dmf)
endif()
