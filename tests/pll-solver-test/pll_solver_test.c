/**
 * @file pll_solver_test.c
 * @brief Unit tests for the STM32 PLL solver's CLK48 (USB/SDIO/SDMMC/RNG)
 * constraint.
 *
 * These build and run natively on the host (arch/x86_64) - stm32_common.c's
 * PLL math is plain arithmetic, and the register-readback path is exercised
 * against a stack-allocated fake RCC_TypeDef instead of real hardware, so
 * none of this needs a target board.
 */
#include "dmod_test.h"
#include "stm32_common.h"
#include "port/stm32_common_regs.h"

/* Mirrors the real per-family limits (see src/port/stm32f7/port.c and
 * src/port/stm32f4/port.c) without depending on either port's headers. */
static const clock_limits_t f7_limits = {
    .max_sysclk = 216000000U, .max_hclk = 216000000U,
    .max_pclk1 = 54000000U, .max_pclk2 = 108000000U,
    .vco_min = 100000000U, .vco_max = 432000000U,
    .pll_in_min = 1000000U, .pll_in_max = 2000000U,
    .pllm_min = 2U, .pllm_max = 63U,
    .plln_min = 50U, .plln_max = 432U,
    .pllp_min = 2U, .pllp_max = 8U,
    .pllq_min = 2U, .pllq_max = 15U,
};

static const clock_limits_t f4_limits = {
    .max_sysclk = 168000000U, .max_hclk = 168000000U,
    .max_pclk1 = 42000000U, .max_pclk2 = 84000000U,
    .vco_min = 100000000U, .vco_max = 432000000U,
    .pll_in_min = 1000000U, .pll_in_max = 2000000U,
    .pllm_min = 2U, .pllm_max = 63U,
    .plln_min = 50U, .plln_max = 432U,
    .pllp_min = 2U, .pllp_max = 8U,
    .pllq_min = 2U, .pllq_max = 15U,
};

void dmod_test_setup(void) {}
void dmod_test_teardown(void) {}

/* STM32F746 at 216 MHz from the STM32F746G-DISCO's 25 MHz HSE must pick
 * PLLQ=9 (VCO=432MHz / 9 = 48MHz) - the exact case from the issue's
 * acceptance criteria. */
DMOD_TEST_STEP(stm32f7_216mhz_from_25mhz_hse_picks_pllq_9)
{
    pll_config_t cfg = {0};
    uint32_t actual_freq = 0;

    int rc = stm32_calculate_pll_config(216000000ULL, 0, 25000000U, &f7_limits, &cfg, &actual_freq);

    DMOD_TEST_EXPECT_EQ(rc, 0);
    DMOD_TEST_EXPECT_EQ(actual_freq, 216000000U);
    DMOD_TEST_EXPECT_EQ(cfg.pllq, 9U);
}

/* Typical STM32F407 at 168 MHz from an 8 MHz HSE must pick PLLQ=7
 * (VCO=336MHz / 7 = 48MHz) - the other acceptance-criteria case. */
DMOD_TEST_STEP(stm32f4_168mhz_from_8mhz_hse_picks_pllq_7)
{
    pll_config_t cfg = {0};
    uint32_t actual_freq = 0;

    int rc = stm32_calculate_pll_config(168000000ULL, 0, 8000000U, &f4_limits, &cfg, &actual_freq);

    DMOD_TEST_EXPECT_EQ(rc, 0);
    DMOD_TEST_EXPECT_EQ(actual_freq, 168000000U);
    DMOD_TEST_EXPECT_EQ(cfg.pllq, 7U);
}

/* A SYSCLK target that is reachable within tolerance but whose VCO never
 * divides down to exactly 48 MHz through a valid PLLQ must fail the whole
 * configuration - not silently fall back to a wrong/default PLLQ, which is
 * exactly the bug this solver rewrite fixes. */
DMOD_TEST_STEP(unsupported_combination_fails_clearly)
{
    pll_config_t cfg = {0};
    uint32_t actual_freq = 0;

    int rc = stm32_calculate_pll_config(100000000ULL, 0, 25000000U, &f7_limits, &cfg, &actual_freq);

    DMOD_TEST_EXPECT_EQ(rc, -1);
}

/* stm32_get_clk48_freq() must derive its answer from the PLLM/PLLN/PLLQ
 * fields actually programmed into RCC_PLLCFGR, not from a cached constant -
 * verified here against a fake register block, not the real oscillator. */
DMOD_TEST_STEP(clk48_readback_derives_from_register_fields)
{
    RCC_TypeDef fake_rcc = {0};
    fake_rcc.CFGR = (2U << RCC_CFGR_SWS_Pos); /* SWS=2: PLL selected as SYSCLK */
    fake_rcc.PLLCFGR = ((25U << RCC_PLLCFGR_PLLM_Pos) & RCC_PLLCFGR_PLLM_Msk)
                     | ((432U << RCC_PLLCFGR_PLLN_Pos) & RCC_PLLCFGR_PLLN_Msk)
                     | ((9U << RCC_PLLCFGR_PLLQ_Pos) & RCC_PLLCFGR_PLLQ_Msk);

    uint32_t freq = stm32_get_clk48_freq((uintptr_t)&fake_rcc, 25000000U);

    DMOD_TEST_EXPECT_EQ(freq, 48000000U);
}

/* Same register block but with a different (wrong) PLLQ programmed - the
 * readback must reflect that, not silently report 48 MHz regardless. */
DMOD_TEST_STEP(clk48_readback_reflects_wrong_pllq)
{
    RCC_TypeDef fake_rcc = {0};
    fake_rcc.CFGR = (2U << RCC_CFGR_SWS_Pos);
    fake_rcc.PLLCFGR = ((25U << RCC_PLLCFGR_PLLM_Pos) & RCC_PLLCFGR_PLLM_Msk)
                     | ((432U << RCC_PLLCFGR_PLLN_Pos) & RCC_PLLCFGR_PLLN_Msk)
                     | ((4U << RCC_PLLCFGR_PLLQ_Pos) & RCC_PLLCFGR_PLLQ_Msk);

    uint32_t freq = stm32_get_clk48_freq((uintptr_t)&fake_rcc, 25000000U);

    DMOD_TEST_EXPECT_EQ(freq, 108000000U);
}

/* When the PLL is not selected as SYSCLK (e.g. still running on HSI/HSE
 * directly), there is no well-defined CLK48 - must report 0, not stale
 * PLLCFGR contents from a previous configuration. */
DMOD_TEST_STEP(clk48_readback_is_zero_when_pll_not_selected)
{
    RCC_TypeDef fake_rcc = {0};
    fake_rcc.CFGR = (0U << RCC_CFGR_SWS_Pos); /* SWS=0: HSI selected */
    fake_rcc.PLLCFGR = ((25U << RCC_PLLCFGR_PLLM_Pos) & RCC_PLLCFGR_PLLM_Msk)
                     | ((432U << RCC_PLLCFGR_PLLN_Pos) & RCC_PLLCFGR_PLLN_Msk)
                     | ((9U << RCC_PLLCFGR_PLLQ_Pos) & RCC_PLLCFGR_PLLQ_Msk);

    uint32_t freq = stm32_get_clk48_freq((uintptr_t)&fake_rcc, 25000000U);

    DMOD_TEST_EXPECT_EQ(freq, 0U);
}
