#define DMOD_ENABLE_REGISTRATION    ON
#include "dmclk_port.h"
#include "../stm32_common/stm32_common.h"
#include "port/stm32_common_regs.h"
#include "port/stm32f7_regs.h"

/* Static storage for current oscillator frequency */
static uint32_t current_hse_freq = 0;
static uint32_t current_sysclk = HSI_VALUE;

/* Clock limits for STM32F7 */
static const clock_limits_t stm32f7_limits = {
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
    .flash_latency_table = stm32f7_flash_latency,
    .flash_latency_count = STM32F7_FLASH_LATENCY_COUNT,
};

/**
 * @brief Initialize the DMDRVI module
 * 
 * @param Config Pointer to Dmod_Config_t structure with configuration parameters
 * 
 * @return int 0 on success, non-zero on failure
 */
int dmod_init(const Dmod_Config_t *Config)
{
    Dmod_Printf("DMDRVI interface module initialized (STM32F7)\n");
    return 0;
}

/**
 * @brief Deinitialize the DMDRVI module
 * 
 * @return int 0 on success, non-zero on failure
 */
int dmod_deinit(void)
{
    Dmod_Printf("DMDRVI interface module deinitialized (STM32F7)\n");
    return 0;
}

/**
 * @brief Configure internal clock source (HSI + PLL)
 * 
 * @param target_freq Target frequency in Hz
 * @param tolerance Tolerance in Hz
 * 
 * @return int 0 on success, non-zero on failure
 */
dmod_dmclk_port_api_declaration(1.0, int, _configure_internal, ( dmclk_frequency_t target_freq, dmclk_frequency_t tolerance) )
{
    volatile RCC_TypeDef *RCC = (RCC_TypeDef *)STM32F7_RCC_BASE;
    pll_config_t pll_config;
    
    /* Enable HSI if not already enabled */
    RCC->CR |= RCC_CR_HSION;
    if (stm32_wait_clock_ready(STM32F7_RCC_BASE, RCC_CR_HSIRDY, HSI_STARTUP_TIMEOUT) != 0) {
        return -1;
    }

    /* Calculate PLL configuration */
    if (stm32_calculate_pll_config(target_freq, tolerance, HSI_VALUE, 
                                    &stm32f7_limits, &pll_config) != 0) {
        return -1;
    }

    /* Configure Flash latency */
    if (stm32_configure_flash_latency(target_freq, STM32F7_FLASH_BASE,
                                      stm32f7_flash_latency, STM32F7_FLASH_LATENCY_COUNT) != 0) {
        return -1;
    }

    /* Disable PLL before configuration */
    RCC->CR &= ~RCC_CR_PLLON;
    while (RCC->CR & RCC_CR_PLLRDY) {
        /* Wait for PLL to unlock */
    }

    /* Configure PLL */
    uint32_t pllcfgr = 0;
    pllcfgr |= (pll_config.pllm << RCC_PLLCFGR_PLLM_Pos) & RCC_PLLCFGR_PLLM_Msk;
    pllcfgr |= (pll_config.plln << RCC_PLLCFGR_PLLN_Pos) & RCC_PLLCFGR_PLLN_Msk;
    pllcfgr |= (((pll_config.pllp / 2) - 1) << RCC_PLLCFGR_PLLP_Pos) & RCC_PLLCFGR_PLLP_Msk;
    pllcfgr |= (pll_config.pllq << RCC_PLLCFGR_PLLQ_Pos) & RCC_PLLCFGR_PLLQ_Msk;
    /* PLL source is HSI (bit 22 = 0) */
    RCC->PLLCFGR = pllcfgr;

    /* Enable PLL */
    RCC->CR |= RCC_CR_PLLON;
    if (stm32_wait_clock_ready(STM32F7_RCC_BASE, RCC_CR_PLLRDY, PLL_STARTUP_TIMEOUT) != 0) {
        return -1;
    }

    /* Configure bus prescalers */
    if (stm32_configure_bus_prescalers(STM32F7_RCC_BASE, target_freq, &stm32f7_limits) != 0) {
        return -1;
    }

    /* Switch system clock to PLL */
    if (stm32_switch_sysclk(STM32F7_RCC_BASE, RCC_CFGR_SW_PLL) != 0) {
        return -1;
    }

    current_sysclk = target_freq;
    return 0;
}

/**
 * @brief Configure external clock source (HSE + PLL)
 * 
 * @param target_freq Target frequency in Hz
 * @param tolerance Tolerance in Hz
 * @param oscillator_freq Oscillator frequency in Hz
 * 
 * @return int 0 on success, non-zero on failure
 */
dmod_dmclk_port_api_declaration(1.0, int, _configure_external, ( dmclk_frequency_t target_freq, dmclk_frequency_t tolerance, dmclk_frequency_t oscillator_freq) )
{
    volatile RCC_TypeDef *RCC = (RCC_TypeDef *)STM32F7_RCC_BASE;
    pll_config_t pll_config;
    
    current_hse_freq = (uint32_t)oscillator_freq;

    /* Enable HSE */
    RCC->CR |= RCC_CR_HSEON;
    if (stm32_wait_clock_ready(STM32F7_RCC_BASE, RCC_CR_HSERDY, HSE_STARTUP_TIMEOUT) != 0) {
        return -1;
    }

    /* Calculate PLL configuration */
    if (stm32_calculate_pll_config(target_freq, tolerance, (uint32_t)oscillator_freq, 
                                    &stm32f7_limits, &pll_config) != 0) {
        return -1;
    }

    /* Configure Flash latency */
    if (stm32_configure_flash_latency(target_freq, STM32F7_FLASH_BASE,
                                      stm32f7_flash_latency, STM32F7_FLASH_LATENCY_COUNT) != 0) {
        return -1;
    }

    /* Disable PLL before configuration */
    RCC->CR &= ~RCC_CR_PLLON;
    while (RCC->CR & RCC_CR_PLLRDY) {
        /* Wait for PLL to unlock */
    }

    /* Configure PLL */
    uint32_t pllcfgr = 0;
    pllcfgr |= (pll_config.pllm << RCC_PLLCFGR_PLLM_Pos) & RCC_PLLCFGR_PLLM_Msk;
    pllcfgr |= (pll_config.plln << RCC_PLLCFGR_PLLN_Pos) & RCC_PLLCFGR_PLLN_Msk;
    pllcfgr |= (((pll_config.pllp / 2) - 1) << RCC_PLLCFGR_PLLP_Pos) & RCC_PLLCFGR_PLLP_Msk;
    pllcfgr |= (pll_config.pllq << RCC_PLLCFGR_PLLQ_Pos) & RCC_PLLCFGR_PLLQ_Msk;
    pllcfgr |= RCC_PLLCFGR_PLLSRC; /* PLL source is HSE (bit 22 = 1) */
    RCC->PLLCFGR = pllcfgr;

    /* Enable PLL */
    RCC->CR |= RCC_CR_PLLON;
    if (stm32_wait_clock_ready(STM32F7_RCC_BASE, RCC_CR_PLLRDY, PLL_STARTUP_TIMEOUT) != 0) {
        return -1;
    }

    /* Configure bus prescalers */
    if (stm32_configure_bus_prescalers(STM32F7_RCC_BASE, target_freq, &stm32f7_limits) != 0) {
        return -1;
    }

    /* Switch system clock to PLL */
    if (stm32_switch_sysclk(STM32F7_RCC_BASE, RCC_CFGR_SW_PLL) != 0) {
        return -1;
    }

    current_sysclk = target_freq;
    return 0;
}

/**
 * @brief Configure hibernation clock source (LSI)
 * 
 * @param target_freq Target frequency in Hz
 * @param tolerance Tolerance in Hz
 * @param oscillator_freq Oscillator frequency in Hz
 * 
 * @return int 0 on success, non-zero on failure
 */
int dmclk_port_configure_hibernatation(dmclk_frequency_t target_freq, dmclk_frequency_t tolerance, dmclk_frequency_t oscillator_freq)
{
    /* LSI is typically used for hibernation/low-power modes
     * For STM32F7, LSI runs at approximately 32 kHz
     * This is a simplified implementation that just returns the LSI frequency
     */
    if (target_freq > LSI_VALUE + tolerance || target_freq < LSI_VALUE - tolerance) {
        return -1; /* Cannot achieve target frequency with LSI */
    }

    current_sysclk = LSI_VALUE;
    return 0;
}

/**
 * @brief Delay for a specified time in microseconds
 * 
 * @param time_us Time to delay in microseconds
 */
void dmclk_port_delay_us(dmclk_time_us_t time_us)
{
    /* Simple delay loop
     * This is a very basic implementation - cycles depend on sysclk
     * For accurate timing, would need to use SysTick or a timer
     */
    uint32_t cycles_per_us = current_sysclk / 1000000U;
    uint32_t cycles = (uint32_t)(time_us * cycles_per_us);
    
    /* Each loop iteration takes approximately 4 cycles */
    cycles /= 4U;
    
    for (uint32_t i = 0; i < cycles; i++) {
        __asm__ volatile ("nop");
    }
}

/**
 * @brief Get the current clock frequency
 * 
 * @return dmclk_frequency_t Current frequency in Hz
 */
dmclk_frequency_t dmclk_port_get_current_frequency(void)
{
    /* Return cached value or calculate from registers */
    uint32_t freq = stm32_get_sysclk_freq(STM32F7_RCC_BASE, HSI_VALUE);
    if (freq > 0) {
        current_sysclk = freq;
    }
    return (dmclk_frequency_t)current_sysclk;
}