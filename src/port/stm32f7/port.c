#define DMOD_ENABLE_REGISTRATION ON
#include "dmclk_port.h"
#include "../stm32_common/clock.h"
#include "port/stm32f7_regs.h"

static clock_limits_t stm32f7_limits = {
    .max_sysclk = STM32F7_MAX_SYSCLK,
    .max_hclk = STM32F7_MAX_HCLK,
    .max_pclk1 = STM32F7_MAX_PCLK1,
    .max_pclk2 = STM32F7_MAX_PCLK2,
    .vco_min = STM32F7_VCO_MIN,
    .vco_max = STM32F7_VCO_MAX,
    .pll_in_min = STM32F7_PLL_IN_MIN,
    .pll_in_max = STM32F7_PLL_IN_MAX,
    .pllm_min = STM32F7_PLLM_MIN,
    .pllm_max = STM32F7_PLLM_MAX,
    .plln_min = STM32F7_PLLN_MIN,
    .plln_max = STM32F7_PLLN_MAX,
    .pllp_min = STM32F7_PLLP_MIN,
    .pllp_max = STM32F7_PLLP_MAX,
    .pllq_min = STM32F7_PLLQ_MIN,
    .pllq_max = STM32F7_PLLQ_MAX,
    .flash_latency_count = STM32F7_FLASH_LATENCY_COUNT,
};

static stm32_clock_config_t clock_config = {
    .rcc_base = STM32F7_RCC_BASE,
    .flash_base = STM32F7_FLASH_BASE,
    .pwr_base = STM32F7_PWR_BASE,
    .overdrive_threshold = STM32F7_MAX_SYSCLK_NO_OVERDRIVE,
    .sai_supported = true,
};

int dmod_init(const Dmod_Config_t *config)
{
    (void)config;
    /* DMF data pointers must be assigned after the module is relocated. */
    stm32f7_limits.flash_latency_table = stm32f7_flash_latency;
    clock_config.limits = &stm32f7_limits;
    stm32_clock_init(&clock_config);
    Dmod_Printf("dmclk port initialized (stm32f7)\n");
    return 0;
}

int dmod_deinit(void)
{
    return stm32_clock_deinit();
}
