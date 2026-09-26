#ifndef DMCLK_STM32_H
#define DMCLK_STM32_H

#include "dmclk_port.h"

/**
 * @brief Get the actual, currently-programmed CLK48 frequency (STM32F4/F7 only).
 *
 * On these families a single PLL Q-divider feeds USB OTG FS, SDIO/SDMMC,
 * and the RNG - one shared domain, not one frequency per consumer, and not
 * a concept every dmclk_port implementation has. This is intentionally
 * NOT part of the generic dmclk_port DIF contract (dmclk_port.h): a future
 * port for a family with a different clock topology (e.g. no unified
 * 48 MHz divider) should not be required to implement it just to satisfy
 * a link dependency of the portable dmclk driver.
 *
 * Read back from RCC_PLLCFGR rather than cached from the last
 * configuration request, the same as dmclk_port_get_current_frequency().
 * Only STM32-aware consumers (e.g. a future dmsdio, which this ecosystem's
 * epic already scopes to STM32F4/F7 exclusively) should link against this.
 *
 * @return dmclk_frequency_t CLK48 frequency in Hz, or 0 if the PLL isn't
 *         currently driving the system clock
 */
dmclk_frequency_t dmclk_stm32_get_clk48_frequency(void);

#endif // DMCLK_STM32_H
