#include "stm32_common.h"
#include "port/stm32_common_regs.h"
#include <stddef.h>

/**
 * @brief Calculate PLL parameters for target frequency
 */
int stm32_calculate_pll_config(dmclk_frequency_t target_freq, 
                                dmclk_frequency_t tolerance,
                                uint32_t source_freq,
                                const clock_limits_t *limits,
                                pll_config_t *config)
{
    if (config == NULL || limits == NULL) {
        return -1;
    }

    /* Cast 64-bit frequencies to 32-bit to avoid 64-bit division on ARM */
    uint32_t target_freq_32 = (uint32_t)target_freq;
    uint32_t tolerance_32 = (uint32_t)tolerance;

    /* Check if target frequency is within limits */
    if (target_freq_32 > limits->max_sysclk) {
        return -1;
    }

    uint32_t best_error = 0xFFFFFFFFU;
    pll_config_t best_config = {0};
    int found = 0;

    /* Try different PLLM values */
    for (uint32_t pllm = limits->pllm_min; pllm <= limits->pllm_max; pllm++) {
        uint32_t pll_in = source_freq / pllm;
        
        /* Check if PLL input frequency is within valid range */
        if (pll_in < limits->pll_in_min || pll_in > limits->pll_in_max) {
            continue;
        }

        /* Try different PLLP values (only 2, 4, 6, 8 are valid) */
        for (uint32_t pllp = limits->pllp_min; pllp <= limits->pllp_max; pllp += 2) {
            /* Calculate required PLLN */
            uint32_t plln = (target_freq_32 * pllp) / pll_in;
            
            /* Check if PLLN is within valid range */
            if (plln < limits->plln_min || plln > limits->plln_max) {
                continue;
            }

            /* Calculate VCO frequency */
            uint32_t vco = pll_in * plln;
            
            /* Check if VCO frequency is within valid range */
            if (vco < limits->vco_min || vco > limits->vco_max) {
                continue;
            }

            /* Calculate actual output frequency */
            uint32_t actual_freq = vco / pllp;
            
            /* Calculate error */
            uint32_t error;
            if (actual_freq > target_freq_32) {
                error = actual_freq - target_freq_32;
            } else {
                error = target_freq_32 - actual_freq;
            }

            /* Check if this is the best configuration so far */
            if (error < best_error && error <= tolerance_32) {
                best_error = error;
                best_config.pllm = pllm;
                best_config.plln = plln;
                best_config.pllp = pllp;
                best_config.pllq = 4; /* Default value for USB, can be optimized */
                found = 1;

                /* Perfect match found */
                if (error == 0) {
                    break;
                }
            }
        }
        
        if (found && best_error == 0) {
            break;
        }
    }

    if (!found) {
        return -1;
    }

    *config = best_config;
    return 0;
}

/**
 * @brief Configure Flash latency based on system clock frequency
 */
int stm32_configure_flash_latency(uint32_t sysclk_freq,
                                   uintptr_t flash_base,
                                   const void *latency_table,
                                   uint32_t table_size)
{
    if (latency_table == NULL) {
        return -1;
    }

    volatile FLASH_TypeDef *FLASH = (FLASH_TypeDef *)flash_base;
    const struct { uint32_t max_freq; uint32_t latency; } *table = 
        (const struct { uint32_t max_freq; uint32_t latency; } *)latency_table;

    uint32_t latency = 0;
    for (uint32_t i = 0; i < table_size; i++) {
        if (sysclk_freq <= table[i].max_freq) {
            latency = table[i].latency;
            break;
        }
    }

    /* Set Flash latency */
    uint32_t acr = FLASH->ACR;
    acr &= ~FLASH_ACR_LATENCY_Msk;
    acr |= (latency << FLASH_ACR_LATENCY_Pos);
    FLASH->ACR = acr;

    /* Verify that the latency was set correctly */
    if ((FLASH->ACR & FLASH_ACR_LATENCY_Msk) != (latency << FLASH_ACR_LATENCY_Pos)) {
        return -1;
    }

    return 0;
}

/**
 * @brief Wait for clock to be ready
 */
int stm32_wait_clock_ready(uintptr_t rcc_base, uint32_t ready_bit, uint32_t timeout)
{
    volatile RCC_TypeDef *RCC = (RCC_TypeDef *)rcc_base;
    uint32_t counter = 0;

    while (!(RCC->CR & ready_bit)) {
        if (++counter > timeout) {
            return -1;
        }
    }

    return 0;
}

/**
 * @brief Switch system clock source
 */
int stm32_switch_sysclk(uintptr_t rcc_base, uint32_t source)
{
    volatile RCC_TypeDef *RCC = (RCC_TypeDef *)rcc_base;
    uint32_t expected_sws;

    /* Set the system clock source */
    uint32_t cfgr = RCC->CFGR;
    cfgr &= ~RCC_CFGR_SW_Msk;
    cfgr |= (source << RCC_CFGR_SW_Pos);
    RCC->CFGR = cfgr;

    /* Calculate expected SWS value */
    expected_sws = source << RCC_CFGR_SWS_Pos;

    /* Wait for clock switch to complete */
    uint32_t counter = 0;
    while ((RCC->CFGR & RCC_CFGR_SWS_Msk) != expected_sws) {
        if (++counter > CLOCKSWITCH_TIMEOUT) {
            return -1;
        }
    }

    return 0;
}

/**
 * @brief Configure bus prescalers
 */
int stm32_configure_bus_prescalers(uintptr_t rcc_base, 
                                    uint32_t sysclk_freq,
                                    const clock_limits_t *limits)
{
    if (limits == NULL) {
        return -1;
    }

    volatile RCC_TypeDef *RCC = (RCC_TypeDef *)rcc_base;
    uint32_t cfgr = RCC->CFGR;

    /* Configure AHB prescaler (HCLK) - typically 1:1 with SYSCLK */
    cfgr &= ~RCC_CFGR_HPRE_Msk;
    cfgr |= (0U << RCC_CFGR_HPRE_Pos); /* Division by 1 */

    /* Configure APB1 prescaler (low-speed bus) */
    uint32_t apb1_div = 1;
    uint32_t apb1_prescaler = 0; /* No division */
    
    while ((sysclk_freq / apb1_div) > limits->max_pclk1) {
        apb1_div *= 2;
        apb1_prescaler++;
        if (apb1_prescaler > 4) { /* Max division is /16 (prescaler = 4) */
            return -1;
        }
    }
    
    if (apb1_prescaler > 0) {
        apb1_prescaler += 3; /* 0->4 (div2), 1->5 (div4), 2->6 (div8), 3->7 (div16) */
    }
    
    cfgr &= ~RCC_CFGR_PPRE1_Msk;
    cfgr |= (apb1_prescaler << RCC_CFGR_PPRE1_Pos);

    /* Configure APB2 prescaler (high-speed bus) */
    uint32_t apb2_div = 1;
    uint32_t apb2_prescaler = 0; /* No division */
    
    while ((sysclk_freq / apb2_div) > limits->max_pclk2) {
        apb2_div *= 2;
        apb2_prescaler++;
        if (apb2_prescaler > 4) { /* Max division is /16 (prescaler = 4) */
            return -1;
        }
    }
    
    if (apb2_prescaler > 0) {
        apb2_prescaler += 3; /* 0->4 (div2), 1->5 (div4), 2->6 (div8), 3->7 (div16) */
    }
    
    cfgr &= ~RCC_CFGR_PPRE2_Msk;
    cfgr |= (apb2_prescaler << RCC_CFGR_PPRE2_Pos);

    RCC->CFGR = cfgr;

    return 0;
}

/**
 * @brief Get current system clock frequency
 */
uint32_t stm32_get_sysclk_freq(uintptr_t rcc_base, uint32_t hsi_value)
{
    volatile RCC_TypeDef *RCC = (RCC_TypeDef *)rcc_base;
    uint32_t sysclk = 0;
    uint32_t sws = (RCC->CFGR & RCC_CFGR_SWS_Msk) >> RCC_CFGR_SWS_Pos;

    switch (sws) {
        case 0: /* HSI */
            sysclk = hsi_value;
            break;
            
        case 1: /* HSE */
            /* Would need to know HSE value, return 0 for now */
            sysclk = 0;
            break;
            
        case 2: /* PLL */
        {
            uint32_t pllcfgr = RCC->PLLCFGR;
            uint32_t pllm = (pllcfgr & RCC_PLLCFGR_PLLM_Msk) >> RCC_PLLCFGR_PLLM_Pos;
            uint32_t plln = (pllcfgr & RCC_PLLCFGR_PLLN_Msk) >> RCC_PLLCFGR_PLLN_Pos;
            uint32_t pllp_bits = (pllcfgr & RCC_PLLCFGR_PLLP_Msk) >> RCC_PLLCFGR_PLLP_Pos;
            uint32_t pllp = (pllp_bits + 1) * 2; /* 0=2, 1=4, 2=6, 3=8 */
            uint32_t pllsrc = (pllcfgr & RCC_PLLCFGR_PLLSRC) ? 1 : 0;
            
            uint32_t pll_input;
            if (pllsrc == 0) {
                pll_input = hsi_value;
            } else {
                /* HSE - would need to know HSE value */
                pll_input = 0;
            }
            
            if (pll_input > 0 && pllm > 0) {
                uint32_t vco = (pll_input / pllm) * plln;
                sysclk = vco / pllp;
            }
            break;
        }
            
        default:
            sysclk = 0;
            break;
    }

    return sysclk;
}
