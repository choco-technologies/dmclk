#ifndef DMCLK_SAI_CLOCK_H
#define DMCLK_SAI_CLOCK_H

#include "dmclk_port.h"
#include <stdbool.h>

typedef struct
{
    uint32_t n;
    uint32_t q;
    uint32_t divq;
    uint32_t frequency;
} stm32_sai_setting_t;

int stm32_sai_solve(uint32_t source, uint32_t m, uint32_t target,
                    uint32_t tolerance, stm32_sai_setting_t *setting);
dmclk_frequency_t stm32_sai_frequency(dmclk_domain_t domain);
int stm32_clock_begin(void);
void stm32_clock_end(void);

#endif
