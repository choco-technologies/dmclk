#ifndef DMCLK_TYPES_H
#define DMCLK_TYPES_H
#include <stdint.h>
/**
 * @brief Clock frequency type in Hz
 */
typedef uint64_t dmclk_frequency_t;

/**
 * @brief Time type in microseconds
 */
typedef uint64_t dmclk_time_us_t;

/**
 * @brief Named peripheral clock domain, for ports that expose more than
 * just the main system clock.
 *
 * Named by what the domain is *for*, not by how any particular family
 * derives it - on STM32F4/F7 all three values below resolve to the same
 * physical signal (one PLL Q-divider), but a port for hardware where they
 * are genuinely independent clocks would return a different frequency for
 * each. A port that has no equivalent for a given value returns 0 for it -
 * every dmclk_port implementation must define _get_domain_frequency(), but
 * "return 0" is a perfectly valid implementation for domains it lacks.
 */
typedef enum
{
    dmclk_domain_sdio = 0,  /**< Clock feeding SDIO/SDMMC peripherals */
    dmclk_domain_usb,       /**< Clock feeding USB (OTG FS/HS) peripherals */
    dmclk_domain_rng,       /**< Clock feeding the RNG peripheral */
    dmclk_domain_sai1,      /**< SAI1 kernel clock (STM32F7) */
    dmclk_domain_sai2,      /**< SAI2 kernel clock (STM32F7) */
} dmclk_domain_t;

#endif
