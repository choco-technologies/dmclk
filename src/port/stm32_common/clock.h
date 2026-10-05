#ifndef DMCLK_STM32_CLOCK_H
#define DMCLK_STM32_CLOCK_H

#include "stm32_common.h"
#include <stdbool.h>

typedef struct
{
    uintptr_t rcc_base;
    uintptr_t flash_base;
    uintptr_t pwr_base;
    const clock_limits_t *limits;
    uint32_t overdrive_threshold;
    bool sai_supported;
} stm32_clock_config_t;

/* Family ports provide addresses and capabilities; all logic stays common. */
void stm32_clock_init(const stm32_clock_config_t *config);
int stm32_clock_deinit(void);
const stm32_clock_config_t *stm32_clock_config(void);
uint32_t stm32_pll_source_frequency(void);

#define STM32_RCC(offset) (*(volatile uint32_t *)(stm32_clock_config()->rcc_base + (offset)))

#endif
