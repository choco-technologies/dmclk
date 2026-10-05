# Both the module and native port tests build this same implementation.
# Keep the source list here, owned by the production STM32 port.
function(dmclk_add_stm32_sources target)
    target_sources(${target} PRIVATE
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/stm32_common.c
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/clock.c
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/sai_clock.c
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/sai_solver.c
    )
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_FUNCTION_LIST_DIR})
endfunction()
