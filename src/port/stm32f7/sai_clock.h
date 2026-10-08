#ifndef STM32F7_SAI_CLOCK_H
#define STM32F7_SAI_CLOCK_H

#include "dmclk_port.h"

typedef struct {
    uint32_t n, q, div;
    dmclk_frequency_t frequency;
} stm32f7_sai_config_t;

int stm32f7_sai_solve(uint32_t source, uint32_t m, dmclk_frequency_t target,
                     dmclk_frequency_t tolerance, stm32f7_sai_config_t *config);
dmclk_frequency_t stm32f7_sai_frequency(uintptr_t base, uint32_t hse, dmclk_domain_t domain);
int stm32f7_sai_acquire(uintptr_t base, uint32_t hse, dmclk_domain_t domain,
                      dmclk_frequency_t target, dmclk_frequency_t tolerance,
                      dmclk_frequency_t *actual);
int stm32f7_sai_release(uintptr_t base, dmclk_domain_t domain);

#endif
