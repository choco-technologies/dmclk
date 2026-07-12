#ifndef STM32_COMMON_H
#define STM32_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include "dmclk_port.h"

/**
 * @brief PLL configuration parameters
 */
typedef struct {
    uint32_t pllm;          /* Division factor for PLL input clock */
    uint32_t plln;          /* Multiplication factor for VCO */
    uint32_t pllp;          /* Division factor for main system clock */
    uint32_t pllq;          /* Division factor for USB OTG FS, SDIO and RNG clocks */
    uint32_t pll_source;    /* PLL source: 0 = HSI, 1 = HSE */
} pll_config_t;

/**
 * @brief Clock configuration limits
 */
typedef struct {
    uint32_t max_sysclk;
    uint32_t max_hclk;
    uint32_t max_pclk1;
    uint32_t max_pclk2;
    uint32_t vco_min;
    uint32_t vco_max;
    uint32_t pll_in_min;
    uint32_t pll_in_max;
    uint32_t pllm_min;
    uint32_t pllm_max;
    uint32_t plln_min;
    uint32_t plln_max;
    uint32_t pllp_min;
    uint32_t pllp_max;
    const void *flash_latency_table;
    uint32_t flash_latency_count;
} clock_limits_t;

/**
 * @brief Common functions for STM32 clock configuration
 */

/**
 * @brief Calculate PLL parameters for target frequency
 * 
 * @param target_freq Target system clock frequency in Hz
 * @param tolerance Tolerance in Hz
 * @param source_freq PLL source frequency (HSI or HSE) in Hz
 * @param limits Clock configuration limits
 * @param config Output PLL configuration
 * @param actual_freq Output actual frequency that will be achieved (can be NULL if not needed)
 * 
 * @return int 0 on success, non-zero on failure
 */
int stm32_calculate_pll_config(dmclk_frequency_t target_freq, 
                                dmclk_frequency_t tolerance,
                                uint32_t source_freq,
                                const clock_limits_t *limits,
                                pll_config_t *config,
                                uint32_t *actual_freq);

/**
 * @brief Configure Flash latency based on system clock frequency
 * 
 * @param sysclk_freq System clock frequency in Hz
 * @param flash_base Flash controller base address
 * @param latency_table Flash latency table
 * @param table_size Size of latency table
 * 
 * @return int 0 on success, non-zero on failure
 */
int stm32_configure_flash_latency(uint32_t sysclk_freq,
                                   uintptr_t flash_base,
                                   const void *latency_table,
                                   uint32_t table_size);

/**
 * @brief Wait for clock to be ready
 * 
 * @param rcc_base RCC base address
 * @param ready_bit Bit position in RCC_CR to check
 * @param timeout Timeout in loop iterations
 * 
 * @return int 0 on success, non-zero on timeout
 */
int stm32_wait_clock_ready(uintptr_t rcc_base, uint32_t ready_bit, uint32_t timeout);

/**
 * @brief Switch system clock source
 * 
 * @param rcc_base RCC base address
 * @param source Clock source (RCC_CFGR_SW_HSI, RCC_CFGR_SW_HSE, or RCC_CFGR_SW_PLL)
 * 
 * @return int 0 on success, non-zero on failure
 */
int stm32_switch_sysclk(uintptr_t rcc_base, uint32_t source);

/**
 * @brief Configure bus prescalers
 * 
 * @param rcc_base RCC base address
 * @param sysclk_freq System clock frequency
 * @param limits Clock configuration limits
 * 
 * @return int 0 on success, non-zero on failure
 */
int stm32_configure_bus_prescalers(uintptr_t rcc_base, 
                                    uint32_t sysclk_freq,
                                    const clock_limits_t *limits);

/**
 * @brief Get current system clock frequency
 * 
 * @param rcc_base RCC base address
 * @param hsi_value HSI oscillator frequency
 * 
 * @return uint32_t Current system clock frequency in Hz
 */
uint32_t stm32_get_sysclk_freq(uintptr_t rcc_base, uint32_t hsi_value);

/**
 * @brief Enable PWR Over-Drive mode (STM32F7 parts only).
 *
 * Required by ST above a family-specific HCLK threshold (216MHz-class parts:
 * 180MHz, see RM0385 "Over-drive switching") to keep the core, buses and
 * peripherals (e.g. FMC to external SDRAM) within timing spec. Enables the
 * PWR peripheral clock, sets PWR_CR1.ODEN and waits for PWR_CSR1.ODRDY.
 *
 * @param rcc_base RCC base address
 * @param pwr_base PWR base address
 * @param timeout Timeout in loop iterations
 *
 * @return int 0 on success, non-zero on timeout
 */
int stm32_enable_overdrive(uintptr_t rcc_base, uintptr_t pwr_base, uint32_t timeout);

/**
 * @brief Delay for a target number of CPU cycles using ARM DWT CYCCNT.
 *
 * The function enables the DWT cycle counter, verifies it is running,
 * then accumulates elapsed cycles (with wrap-around handling) until
 * @p target_cycles is reached.
 *
 * @param target_cycles Number of cycles to wait
 * @param elapsed_cycles Output elapsed cycles measured by DWT
 *
 * @return int 0 on success, non-zero if DWT CYCCNT is unavailable
 */
int stm32_delay_cycles_dwt(uint64_t target_cycles, uint64_t *elapsed_cycles);

#endif // STM32_COMMON_H
