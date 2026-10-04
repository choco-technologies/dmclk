#ifndef DMCLK_SAI_CLOCK_H
#define DMCLK_SAI_CLOCK_H
#include "dmclk_port.h"
#include <stdbool.h>
typedef struct { uint32_t n, q, divq, frequency; } stm32f7_sai_setting_t;
int stm32f7_sai_solve(uint32_t source, uint32_t m, uint32_t target,
                     uint32_t tolerance, stm32f7_sai_setting_t *setting);
uint32_t stm32f7_pll_source_frequency(void);
dmclk_frequency_t stm32f7_sai_frequency(dmclk_domain_t domain);
int stm32f7_clock_begin(void);
void stm32f7_clock_end(void);
#endif
