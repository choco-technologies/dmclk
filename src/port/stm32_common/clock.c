#include "clock.h"
#include "sai_clock.h"
#include "port/stm32_common_regs.h"
#include <errno.h>

static const stm32_clock_config_t *config;
static uint32_t current_hse_freq;
static uint32_t current_sysclk = HSI_VALUE;

void stm32_clock_init(const stm32_clock_config_t *family)
{
    config = family;
    current_hse_freq = 0;
    current_sysclk = HSI_VALUE;
}

const stm32_clock_config_t *stm32_clock_config(void)
{
    return config;
}

int stm32_clock_deinit(void)
{
    int result = stm32_clock_begin();
    if (result != 0)
        return result;
    stm32_clock_end();
    return 0;
}

uint32_t stm32_pll_source_frequency(void)
{
    return (STM32_RCC(RCC_PLLCFGR_OFFSET) & RCC_PLLCFGR_PLLSRC)
        ? current_hse_freq : HSI_VALUE;
}

static int program_main_pll(const pll_config_t *pll, bool external)
{
    volatile RCC_TypeDef *rcc = (RCC_TypeDef *)config->rcc_base;
    rcc->CR &= ~RCC_CR_PLLON;
    for (unsigned i = 0; rcc->CR & RCC_CR_PLLRDY; i++)
    {
        if (i == PLL_STARTUP_TIMEOUT)
            return -ETIMEDOUT;
    }
    uint32_t value = (pll->pllm << RCC_PLLCFGR_PLLM_Pos) & RCC_PLLCFGR_PLLM_Msk;
    value |= (pll->plln << RCC_PLLCFGR_PLLN_Pos) & RCC_PLLCFGR_PLLN_Msk;
    value |= ((pll->pllp / 2 - 1) << RCC_PLLCFGR_PLLP_Pos) & RCC_PLLCFGR_PLLP_Msk;
    value |= (pll->pllq << RCC_PLLCFGR_PLLQ_Pos) & RCC_PLLCFGR_PLLQ_Msk;
    if (external)
        value |= RCC_PLLCFGR_PLLSRC;
    rcc->PLLCFGR = value;
    rcc->CR |= RCC_CR_PLLON;
    return stm32_wait_clock_ready(config->rcc_base, RCC_CR_PLLRDY, PLL_STARTUP_TIMEOUT);
}

static int apply_main_clock(const pll_config_t *pll, uint32_t actual, bool external)
{
    const clock_limits_t *limits = config->limits;
    if (stm32_configure_flash_latency(actual, config->flash_base,
            limits->flash_latency_table, limits->flash_latency_count) != 0)
        return -1;
    if (program_main_pll(pll, external) != 0)
        return -1;
    if (config->overdrive_threshold && actual > config->overdrive_threshold)
    {
        if (stm32_enable_overdrive(config->rcc_base, config->pwr_base,
                OVERDRIVE_STARTUP_TIMEOUT) != 0)
            return -1;
    }
    if (stm32_configure_bus_prescalers(config->rcc_base, actual, limits) != 0)
        return -1;
    return stm32_switch_sysclk(config->rcc_base, RCC_CFGR_SW_PLL);
}

static int configure_clock(dmclk_frequency_t target, dmclk_frequency_t tolerance,
                           uint32_t source, bool external)
{
    volatile RCC_TypeDef *rcc = (RCC_TypeDef *)config->rcc_base;
    uint32_t ready = external ? RCC_CR_HSERDY : RCC_CR_HSIRDY;
    uint32_t timeout = external ? HSE_STARTUP_TIMEOUT : HSI_STARTUP_TIMEOUT;
    rcc->CR |= external ? RCC_CR_HSEON : RCC_CR_HSION;
    if (stm32_wait_clock_ready(config->rcc_base, ready, timeout) != 0)
        return -1;
    pll_config_t pll;
    uint32_t actual;
    /* All configuration helpers receive source explicitly. They do not read
     * current_hse_freq; publish the cached frequency only after success. */
    if (stm32_calculate_pll_config(target, tolerance, source,
            config->limits, &pll, &actual) != 0)
        return -1;
    int result = apply_main_clock(&pll, actual, external);
    if (result == 0)
    {
        if (external)
            current_hse_freq = source;
        current_sysclk = actual;
    }
    return result;
}

dmod_dmclk_port_api_declaration(1.0, int, _configure_internal,
    (dmclk_frequency_t target, dmclk_frequency_t tolerance))
{
    int result = stm32_clock_begin();
    if (result != 0)
        return result;
    result = configure_clock(target, tolerance, HSI_VALUE, false);
    stm32_clock_end();
    return result;
}

dmod_dmclk_port_api_declaration(1.0, int, _configure_external,
    (dmclk_frequency_t target, dmclk_frequency_t tolerance, dmclk_frequency_t source))
{
    int result = stm32_clock_begin();
    if (result != 0)
        return result;
    result = configure_clock(target, tolerance, (uint32_t)source, true);
    stm32_clock_end();
    return result;
}

dmod_dmclk_port_api_declaration(1.0, int, _configure_hibernatation,
    (dmclk_frequency_t target, dmclk_frequency_t tolerance, dmclk_frequency_t source))
{
    (void)source;
    int result = stm32_clock_begin();
    if (result != 0)
        return result;
    uint64_t error = target > LSI_VALUE ? target - LSI_VALUE : LSI_VALUE - target;
    if (error > tolerance)
        result = -1;
    else
        current_sysclk = LSI_VALUE;
    stm32_clock_end();
    return result;
}

dmod_dmclk_port_api_declaration(1.0, dmclk_frequency_t, _get_current_frequency, (void))
{
    uint32_t frequency = stm32_get_sysclk_freq(config->rcc_base, HSI_VALUE);
    if (frequency > 0)
        current_sysclk = frequency;
    return current_sysclk;
}

dmod_dmclk_port_api_declaration(1.0, dmclk_frequency_t, _get_domain_frequency,
    (dmclk_domain_t domain))
{
    switch (domain)
    {
        case dmclk_domain_sai1:
        case dmclk_domain_sai2:
            return config->sai_supported ? stm32_sai_frequency(domain) : 0;
        case dmclk_domain_sdio:
        case dmclk_domain_usb:
        case dmclk_domain_rng:
            return stm32_get_clk48_freq(config->rcc_base, stm32_pll_source_frequency());
        default:
            return 0;
    }
}

dmod_dmclk_port_api_declaration(1.0, void, _delay_us, (dmclk_time_us_t time_us))
{
    uint32_t cycles = (uint32_t)(time_us * (current_sysclk / 1000000U)) / 4U;
    for (uint32_t i = 0; i < cycles; i++)
        __asm__ volatile ("nop");
}

dmod_dmclk_port_api_declaration(1.0, uint64_t, _delay, (uint32_t seconds))
{
    uint64_t target = (uint64_t)current_sysclk * seconds;
    if (!target)
        return 0;
    Dmod_EnterCritical();
    uint64_t elapsed;
    if (stm32_delay_cycles_dwt(target, &elapsed) == 0)
    {
        Dmod_ExitCritical();
        return elapsed;
    }
    uint32_t iterations = current_sysclk / 2U;
    for (uint32_t s = 0; s < seconds; s++)
    {
#if defined(__arm__) || defined(__thumb__)
        uint32_t count = iterations;
        __asm__ volatile ("1: subs %0, %0, #1\n\t bne 1b\n\t" : "+r" (count) : : "cc");
#else
        for (uint32_t i = 0; i < iterations; i++)
            __asm__ volatile ("nop");
#endif
    }
    Dmod_ExitCritical();
    return (uint64_t)iterations * seconds * 2U;
}
