#include "stm32_common.h"
#include "port/stm32_common_regs.h"
#include <stddef.h>

/* ARM CoreSight / DWT registers for cycle-accurate timing on Cortex-M */
#define ARM_DEMCR_ADDR                  0xE000EDFCUL
#define ARM_DEMCR_TRCENA_Msk            (1UL << 24)
#define ARM_DWT_CTRL_ADDR               0xE0001000UL
#define ARM_DWT_CYCCNT_ADDR             0xE0001004UL
#define ARM_DWT_CTRL_CYCCNTENA_Msk      (1UL << 0)
#define ARM_DWT_LAR_ADDR                0xE0001FB0UL
#define ARM_DWT_LAR_UNLOCK_KEY          0xC5ACCE55UL

#define ARM_DEMCR                       (*(volatile uint32_t *)ARM_DEMCR_ADDR)
#define ARM_DWT_CTRL                    (*(volatile uint32_t *)ARM_DWT_CTRL_ADDR)
#define ARM_DWT_CYCCNT                  (*(volatile uint32_t *)ARM_DWT_CYCCNT_ADDR)
#define ARM_DWT_LAR                     (*(volatile uint32_t *)ARM_DWT_LAR_ADDR)

static int stm32_dwt_cyccnt_is_running(void)
{
    uint32_t probe_start = ARM_DWT_CYCCNT;
    __asm__ volatile ("nop\n\t" "nop\n\t" "nop\n\t" "nop\n\t");
    return (ARM_DWT_CYCCNT != probe_start);
}

/**
 * @brief Calculate PLL parameters for target frequency
 */
int stm32_calculate_pll_config(dmclk_frequency_t target_freq, 
                                dmclk_frequency_t tolerance,
                                uint32_t source_freq,
                                const clock_limits_t *limits,
                                pll_config_t *config,
                                uint32_t *actual_freq)
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

    uint32_t best_sysclk_error = 0xFFFFFFFFU;
    uint32_t best_clk48_error = 0xFFFFFFFFU;
    uint32_t best_actual_freq = 0;
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
            uint32_t calc_actual_freq = vco / pllp;

            /* Calculate error */
            uint32_t sysclk_error;
            if (calc_actual_freq > target_freq_32) {
                sysclk_error = calc_actual_freq - target_freq_32;
            } else {
                sysclk_error = target_freq_32 - calc_actual_freq;
            }

            /* SYSCLK accuracy is non-negotiable: candidates outside the
             * caller's tolerance are never acceptable, no matter how good
             * their CLK48 would be. */
            if (sysclk_error > tolerance_32) {
                continue;
            }

            /* This VCO does not have to divide evenly by any PLLQ - for
             * some (SYSCLK, source) combinations no PLLQ can reach exactly
             * 48 MHz within the VCO limits (e.g. 180 MHz from an 8 MHz HSE
             * only ever reaches VCO=360MHz, and 360/48 is not an integer).
             * Rather than failing SYSCLK configuration entirely over that,
             * pick whichever PLLQ gets closest and let the caller read the
             * real result back via dmclk_port_get_domain_frequency(). */
            uint32_t best_pllq_for_vco = 0;
            uint32_t best_clk48_error_for_vco = 0xFFFFFFFFU;
            for (uint32_t pllq = limits->pllq_min; pllq <= limits->pllq_max; pllq++) {
                uint32_t clk48 = vco / pllq;
                uint32_t clk48_error;
                if (clk48 > STM32_CLK48_TARGET_HZ) {
                    clk48_error = clk48 - STM32_CLK48_TARGET_HZ;
                } else {
                    clk48_error = STM32_CLK48_TARGET_HZ - clk48;
                }
                if (clk48_error < best_clk48_error_for_vco) {
                    best_clk48_error_for_vco = clk48_error;
                    best_pllq_for_vco = pllq;
                }
            }

            /* Rank by SYSCLK accuracy first (required to even get here
             * within tolerance, but candidates can still differ), then by
             * CLK48 accuracy as the tiebreaker between SYSCLK-equivalent
             * candidates - e.g. more than one PLLM can hit the exact same
             * SYSCLK through a different VCO with a better 48 MHz fit. */
            int is_better = (sysclk_error < best_sysclk_error) ||
                            (sysclk_error == best_sysclk_error && best_clk48_error_for_vco < best_clk48_error);

            if (is_better) {
                best_sysclk_error = sysclk_error;
                best_clk48_error = best_clk48_error_for_vco;
                best_actual_freq = calc_actual_freq;
                best_config.pllm = pllm;
                best_config.plln = plln;
                best_config.pllp = pllp;
                best_config.pllq = best_pllq_for_vco;
                found = 1;
            }
        }
    }

    if (!found) {
        /* No (PLLM, PLLN, PLLP) can reach the target SYSCLK within
         * tolerance at all - fail clearly rather than programming
         * something out of spec. */
        return -1;
    }

    *config = best_config;
    if (actual_freq != NULL) {
        *actual_freq = best_actual_freq;
    }
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
 * @brief Wait for a disabled clock to unlock
 */
int stm32_wait_clock_stopped(uintptr_t rcc_base, uint32_t ready_bit, uint32_t timeout)
{
    volatile RCC_TypeDef *rcc = (RCC_TypeDef *)rcc_base;
    while (timeout--) {
        if (!(rcc->CR & ready_bit)) { return 0; }
    }
    return -1;
}

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
 * @brief Enable PWR Over-Drive mode
 */
int stm32_enable_overdrive(uintptr_t rcc_base, uintptr_t pwr_base, uint32_t timeout)
{
    volatile RCC_TypeDef *RCC = (RCC_TypeDef *)rcc_base;
    volatile PWR_TypeDef *PWR = (PWR_TypeDef *)pwr_base;
    uint32_t counter;

    RCC->APB1ENR |= RCC_APB1ENR_PWREN;

    PWR->CR1 |= PWR_CR1_ODEN;
    counter = 0;
    while (!(PWR->CSR1 & PWR_CSR1_ODRDY)) {
        if (++counter > timeout) {
            return -1;
        }
    }

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
            /* Note: HSE value must be stored by port implementation in current_hse_freq */
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
                /* HSE - Note: HSE value must be stored by port implementation in current_hse_freq */
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

uint32_t stm32_get_clk48_freq(uintptr_t rcc_base, uint32_t pll_input_freq)
{
    volatile RCC_TypeDef *RCC = (RCC_TypeDef *)rcc_base;
    uint32_t sws = (RCC->CFGR & RCC_CFGR_SWS_Msk) >> RCC_CFGR_SWS_Pos;

    /* CLK48 only exists while the PLL is actually driving the system -
     * with HSI/HSE selected directly the PLL (and its Q-divider) may be
     * off or configured for something else entirely. */
    if (sws != 2 || pll_input_freq == 0U) {
        return 0U;
    }

    uint32_t pllcfgr = RCC->PLLCFGR;
    uint32_t pllm = (pllcfgr & RCC_PLLCFGR_PLLM_Msk) >> RCC_PLLCFGR_PLLM_Pos;
    uint32_t plln = (pllcfgr & RCC_PLLCFGR_PLLN_Msk) >> RCC_PLLCFGR_PLLN_Pos;
    uint32_t pllq = (pllcfgr & RCC_PLLCFGR_PLLQ_Msk) >> RCC_PLLCFGR_PLLQ_Pos;

    if (pllm == 0U || pllq == 0U) {
        return 0U;
    }

    uint32_t vco = (pll_input_freq / pllm) * plln;
    return vco / pllq;
}

int stm32_delay_cycles_dwt(uint64_t target_cycles, uint64_t *elapsed_cycles)
{
    if (elapsed_cycles == NULL) {
        return -1;
    }

    *elapsed_cycles = 0U;
    if (target_cycles == 0U) {
        return 0;
    }

    /* Enable tracing + DWT cycle counter */
    ARM_DEMCR |= ARM_DEMCR_TRCENA_Msk;
    ARM_DWT_LAR = ARM_DWT_LAR_UNLOCK_KEY;
    ARM_DWT_CYCCNT = 0U;
    ARM_DWT_CTRL |= ARM_DWT_CTRL_CYCCNTENA_Msk;

    if (!stm32_dwt_cyccnt_is_running()) {
        return -1;
    }

    uint64_t elapsed = 0U;
    uint32_t prev = ARM_DWT_CYCCNT;

    while (elapsed < target_cycles) {
        uint32_t now = ARM_DWT_CYCCNT;
        elapsed += (uint32_t)(now - prev);
        prev = now;
    }

    *elapsed_cycles = elapsed;
    return 0;
}
