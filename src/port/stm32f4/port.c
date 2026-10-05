#define DMOD_ENABLE_REGISTRATION ON
#include "dmclk_port.h"
#include "../stm32_common/clock.h"
#include "port/stm32f4_regs.h"

static clock_limits_t stm32f4_limits = {
    .max_sysclk = STM32F4_MAX_SYSCLK,
    .max_hclk = STM32F4_MAX_HCLK,
    .max_pclk1 = STM32F4_MAX_PCLK1,
    .max_pclk2 = STM32F4_MAX_PCLK2,
    .vco_min = STM32F4_VCO_MIN,
    .vco_max = STM32F4_VCO_MAX,
    .pll_in_min = STM32F4_PLL_IN_MIN,
    .pll_in_max = STM32F4_PLL_IN_MAX,
    .pllm_min = STM32F4_PLLM_MIN,
    .pllm_max = STM32F4_PLLM_MAX,
    .plln_min = STM32F4_PLLN_MIN,
    .plln_max = STM32F4_PLLN_MAX,
    .pllp_min = STM32F4_PLLP_MIN,
    .pllp_max = STM32F4_PLLP_MAX,
    .pllq_min = STM32F4_PLLQ_MIN,
    .pllq_max = STM32F4_PLLQ_MAX,
    .flash_latency_count = STM32F4_FLASH_LATENCY_COUNT,
};

static stm32_clock_config_t clock_config = {
    .rcc_base = STM32F4_RCC_BASE,
    .flash_base = STM32F4_FLASH_BASE,
};

int dmod_init(const Dmod_Config_t *config)
{
    (void)config;
    /* DMF data pointers must be assigned after the module is relocated. */
    stm32f4_limits.flash_latency_table = stm32f4_flash_latency;
    clock_config.limits = &stm32f4_limits;
    stm32_clock_init(&clock_config);
    Dmod_Printf("dmclk port initialized (stm32f4)\n");
    return 0;
}

int dmod_deinit(void)
{
    return stm32_clock_deinit();
}
