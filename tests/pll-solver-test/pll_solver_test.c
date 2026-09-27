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
    .max_sysclk = 180000000U, .max_hclk = 180000000U,
    .max_pclk1 = 45000000U, .max_pclk2 = 90000000U,
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

/* A SYSCLK target with zero tolerance that no (PLLM, PLLN, PLLP) can reach
 * at all must fail clearly. */
DMOD_TEST_STEP(unreachable_sysclk_fails_clearly)
{
    pll_config_t cfg = {0};
    uint32_t actual_freq = 0;

    int rc = stm32_calculate_pll_config(215999999ULL, 0, 25000000U, &f7_limits, &cfg, &actual_freq);

    DMOD_TEST_EXPECT_EQ(rc, -1);
}

/* SYSCLK accuracy is non-negotiable and must never be sacrificed for a
 * better CLK48 - but CLK48 itself is best-effort: 180 MHz from an 8 MHz HSE
 * can only ever reach VCO=360MHz (VCO=720MHz for PLLP=4 exceeds the 432MHz
 * VCO ceiling), and 360/48 is not an integer, so no PLLQ hits exactly
 * 48 MHz. The solver must still land exactly on 180MHz and pick the
 * closest achievable CLK48 (45MHz via PLLQ=8) instead of failing the whole
 * configuration - this exact (family, target, source) triple is real:
 * see configs/mcu/stm32f429zi.ini and 6 other boards/MCUs in this repo. */
DMOD_TEST_STEP(sysclk_exact_clk48_best_effort_when_unreachable)
{
    pll_config_t cfg = {0};
    uint32_t actual_freq = 0;

    int rc = stm32_calculate_pll_config(180000000ULL, 0, 8000000U, &f4_limits, &cfg, &actual_freq);

    DMOD_TEST_EXPECT_EQ(rc, 0);
    DMOD_TEST_EXPECT_EQ(actual_freq, 180000000U);
    DMOD_TEST_EXPECT_EQ(cfg.pllq, 8U);
}

/* Regression coverage for every (family, target SYSCLK, HSE) combination
 * actually configured somewhere in this repo (configs/mcu/*.ini and
 * configs/board/*.ini) - the solver rewrite that made CLK48 a hard
 * constraint originally broke 8 of these outright (100/180 MHz targets
 * have no exact-48MHz solution - see the test above.) SYSCLK must always
 * land exactly on target regardless of what CLK48 ends up being. */
typedef struct { uint64_t target; uint32_t osc; int is_f7; } known_config_t;

static const known_config_t known_configs[] = {
    /* stm32f401re, nucleo-f401re */         {84000000ULL,  8000000U, 0},
    /* stm32f405rg, stm32f407vg, stm32f4-discovery */ {168000000ULL, 8000000U, 0},
    /* stm32f411re, nucleo-f411re */         {100000000ULL, 8000000U, 0},
    /* stm32f429zi, f439zi, f446re, f469ni, nucleo-f446re, f429i-discovery */ {180000000ULL, 8000000U, 0},
    /* stm32f722re, f746zg, f767zi, f769ni, nucleo-f767zi */ {216000000ULL, 8000000U, 1},
    /* stm32f746g-disco, stm32f769i-discovery */ {216000000ULL, 25000000U, 1},
};

DMOD_TEST_STEP(every_known_board_config_reaches_exact_sysclk)
{
    size_t n = sizeof(known_configs) / sizeof(known_configs[0]);
    for (size_t i = 0; i < n; i++) {
        const known_config_t* kc = &known_configs[i];
        const clock_limits_t* limits = kc->is_f7 ? &f7_limits : &f4_limits;
        pll_config_t cfg = {0};
        uint32_t actual_freq = 0;

        int rc = stm32_calculate_pll_config(kc->target, 1000, kc->osc, limits, &cfg, &actual_freq);

        DMOD_TEST_EXPECT_EQ(rc, 0);
        DMOD_TEST_EXPECT_EQ(actual_freq, (uint32_t)kc->target);
    }
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
