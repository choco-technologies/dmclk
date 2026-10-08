#include "dmod_test.h"
#include "domain.h"
#include "stm32f7/sai_clock.h"
#include "port/stm32f7_regs.h"
#include <errno.h>
#include <string.h>

/* Fake RCC plus explicit hardware-ready transitions. The production solver,
 * readback, register writes, ownership and public reservation API run intact. */
static uint32_t rcc[0x94 / 4];
static int fail_start, fail_stop;
#define BASE ((uintptr_t)rcc)
#define REG(offset) rcc[(offset) / 4]

int stm32_wait_clock_ready(uintptr_t base, uint32_t bit, uint32_t timeout)
{
    (void)timeout;
    if (fail_start) { return -1; }
    ((RCC_TypeDef *)base)->CR |= bit;
    return 0;
}

int stm32_wait_clock_stopped(uintptr_t base, uint32_t bit, uint32_t timeout)
{
    (void)timeout;
    if (fail_stop) { return -1; }
    ((RCC_TypeDef *)base)->CR &= ~bit;
    return 0;
}

dmclk_frequency_t dmclk_port_get_domain_frequency(dmclk_domain_t domain)
{
    if (domain <= dmclk_domain_rng) { return 48000000; }
    return stm32f7_sai_frequency(BASE, 25000000, domain);
}

int dmclk_domain_acquire_hardware(dmclk_domain_t domain, dmclk_frequency_t target,
                                  dmclk_frequency_t tolerance, dmclk_frequency_t *actual)
{
    if (domain <= dmclk_domain_rng) {
        if (!dmclk_domain_matches(48000000, target, tolerance)) { return -ERANGE; }
        *actual = 48000000;
        return 0;
    }
    return stm32f7_sai_acquire(BASE, 25000000, domain, target, tolerance, actual);
}

int dmclk_domain_release_hardware(dmclk_domain_t domain)
{
    return domain <= dmclk_domain_rng ? 0 : stm32f7_sai_release(BASE, domain);
}

static int registers_differ(const uint32_t *before)
{
    for (unsigned i = 0; i < sizeof(rcc) / sizeof(rcc[0]); ++i) {
        if (before[i] != rcc[i]) { return 1; }
    }
    return 0;
}

void dmod_test_setup(void)
{
    memset(rcc, 0, sizeof(rcc));
    fail_start = fail_stop = 0;
    REG(0) = RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY;
    REG(4) = 25 | (432U << 6) | (9U << 24) | RCC_PLLCFGR_PLLSRC;
    REG(8) = RCC_CFGR_SWS_PLL | (5U << RCC_CFGR_PPRE1_Pos);
    REG(STM32F7_RCC_PLLSAICFGR_OFFSET) = (192U << 6) | (4U << 24) | (3U << 28);
    REG(STM32F7_RCC_DCKCFGR1_OFFSET) = (2U << 16) | (1U << 24);
}

void dmod_test_teardown(void)
{
    fail_start = fail_stop = 0;
    for (unsigned d = 0; d <= dmclk_domain_sai2; ++d) {
        while (dmclk_port_release_domain((dmclk_domain_t)d) == 0) {}
    }
}

DMOD_TEST_STEP(acquire_share_conflict_release_preserves_existing_clocks)
{
    uint32_t before[sizeof(rcc) / sizeof(rcc[0])];
    memcpy(before, rcc, sizeof(rcc));
    dmclk_frequency_t actual = 0, unchanged = 123;
    DMOD_TEST_EXPECT_EQ(dmclk_port_acquire_domain(dmclk_domain_sai1, 12288000, 3000, &actual), 0);
    DMOD_TEST_EXPECT_TRUE(dmclk_domain_matches(actual, 12288000, 3000));
    DMOD_TEST_EXPECT_EQ(actual, dmclk_port_get_domain_frequency(dmclk_domain_sai1));
    DMOD_TEST_EXPECT_EQ(REG(4), before[1]);
    DMOD_TEST_EXPECT_EQ(REG(8), before[2]);
    DMOD_TEST_EXPECT_EQ(dmclk_domain_begin_configuration(), -EBUSY);
    DMOD_TEST_EXPECT_EQ(dmclk_port_acquire_domain(dmclk_domain_sai1, 11289600, 1000, &unchanged), -EBUSY);
    DMOD_TEST_EXPECT_EQ(unchanged, 123U);
    DMOD_TEST_EXPECT_EQ(dmclk_port_acquire_domain(dmclk_domain_sai1, 12288000, 3000, &actual), 0);
    DMOD_TEST_EXPECT_EQ(dmclk_port_acquire_domain(dmclk_domain_sai2, 11289600, 1000, &unchanged), 0);
    DMOD_TEST_EXPECT_EQ(dmclk_port_release_domain(dmclk_domain_sai2), 0);
    DMOD_TEST_EXPECT_EQ(dmclk_port_acquire_domain(dmclk_domain_sai2, 12288000, 3000, &actual), 0);
    DMOD_TEST_EXPECT_EQ(dmclk_port_release_domain(dmclk_domain_sai1), 0);
    DMOD_TEST_EXPECT_EQ(dmclk_port_release_domain(dmclk_domain_sai1), 0);
    DMOD_TEST_EXPECT_EQ(dmclk_port_get_domain_frequency(dmclk_domain_sai2), actual);
    DMOD_TEST_EXPECT_EQ(dmclk_port_release_domain(dmclk_domain_sai2), 0);
    DMOD_TEST_EXPECT_EQ(registers_differ(before), 0);
    DMOD_TEST_EXPECT_EQ(dmclk_port_release_domain(dmclk_domain_sai2), -EINVAL);
    DMOD_TEST_EXPECT_EQ(dmclk_domain_begin_configuration(), 0);
    dmclk_domain_end_configuration();
}

DMOD_TEST_STEP(invalid_and_unreachable_requests_leave_hardware_untouched)
{
    uint32_t before[sizeof(rcc) / sizeof(rcc[0])];
    memcpy(before, rcc, sizeof(rcc));
    dmclk_frequency_t actual = 77;
    DMOD_TEST_EXPECT_EQ(dmclk_port_acquire_domain((dmclk_domain_t)-1, 1, 0, &actual), -EINVAL);
    DMOD_TEST_EXPECT_EQ(dmclk_port_acquire_domain((dmclk_domain_t)99, 1, 0, &actual), -EINVAL);
    DMOD_TEST_EXPECT_EQ(dmclk_port_acquire_domain(dmclk_domain_sai1, 0, 0, &actual), -EINVAL);
    DMOD_TEST_EXPECT_EQ(dmclk_port_acquire_domain(dmclk_domain_sai1, 1, 0, NULL), -EINVAL);
    DMOD_TEST_EXPECT_EQ(dmclk_port_acquire_domain(dmclk_domain_sai1, UINT64_MAX, 0, &actual), -ERANGE);
    DMOD_TEST_EXPECT_EQ(dmclk_port_acquire_domain(dmclk_domain_sai1, 12288000, 0, &actual), -ERANGE);
    DMOD_TEST_EXPECT_EQ(actual, 77U);
    DMOD_TEST_EXPECT_EQ(registers_differ(before), 0);
}

DMOD_TEST_STEP(start_timeout_rolls_back_and_can_be_retried)
{
    uint32_t before[sizeof(rcc) / sizeof(rcc[0])];
    memcpy(before, rcc, sizeof(rcc));
    dmclk_frequency_t actual = 77;
    fail_start = 1;
    DMOD_TEST_EXPECT_EQ(dmclk_port_acquire_domain(dmclk_domain_sai1, 12000000, 0, &actual), -ETIMEDOUT);
    DMOD_TEST_EXPECT_EQ(actual, 77U);
    DMOD_TEST_EXPECT_EQ(registers_differ(before), 0);
    DMOD_TEST_EXPECT_EQ(dmclk_port_release_domain(dmclk_domain_sai1), -EINVAL);
    fail_start = 0;
    DMOD_TEST_EXPECT_EQ(dmclk_port_acquire_domain(dmclk_domain_sai1, 12000000, 0, &actual), 0);
    fail_stop = 1;
    DMOD_TEST_EXPECT_EQ(dmclk_port_release_domain(dmclk_domain_sai1), -ETIMEDOUT);
    DMOD_TEST_EXPECT_EQ(dmclk_domain_begin_configuration(), -EBUSY);
    DMOD_TEST_EXPECT_EQ(dmclk_port_get_domain_frequency(dmclk_domain_sai1), 12000000U);
    fail_stop = 0;
    DMOD_TEST_EXPECT_EQ(dmclk_port_release_domain(dmclk_domain_sai1), 0);
    DMOD_TEST_EXPECT_EQ(registers_differ(before), 0);
}

DMOD_TEST_STEP(borrowed_pll_is_never_retuned_or_stopped)
{
    REG(0) |= STM32F7_RCC_PLLSAION | STM32F7_RCC_PLLSAIRDY;
    uint32_t before[sizeof(rcc) / sizeof(rcc[0])];
    memcpy(before, rcc, sizeof(rcc));
    dmclk_frequency_t actual = 0;
    DMOD_TEST_EXPECT_EQ(dmclk_port_acquire_domain(dmclk_domain_sai1, 12288000, 3000, &actual), 0);
    DMOD_TEST_EXPECT_EQ(REG(STM32F7_RCC_PLLSAICFGR_OFFSET), before[STM32F7_RCC_PLLSAICFGR_OFFSET / 4]);
    DMOD_TEST_EXPECT_EQ(dmclk_port_release_domain(dmclk_domain_sai1), 0);
    DMOD_TEST_EXPECT_EQ(dmclk_port_acquire_domain(dmclk_domain_sai1, 48000000, 0, &actual), 0);
    DMOD_TEST_EXPECT_EQ(dmclk_port_release_domain(dmclk_domain_sai1), 0);
    DMOD_TEST_EXPECT_EQ(registers_differ(before), 0);
}

DMOD_TEST_STEP(independent_auxiliary_sources_and_external_input)
{
    REG(STM32F7_RCC_DCKCFGR1_OFFSET) |= 1U << STM32F7_RCC_SAI2SEL_Pos;
    dmclk_frequency_t first, second;
    DMOD_TEST_EXPECT_EQ(dmclk_port_acquire_domain(dmclk_domain_sai1, 12288000, 3000, &first), 0);
    DMOD_TEST_EXPECT_EQ(dmclk_port_acquire_domain(dmclk_domain_sai2, 11289600, 1000, &second), 0);
    DMOD_TEST_EXPECT_EQ(dmclk_port_release_domain(dmclk_domain_sai1), 0);
    DMOD_TEST_EXPECT_EQ(dmclk_port_get_domain_frequency(dmclk_domain_sai2), second);
    DMOD_TEST_EXPECT_EQ(dmclk_port_release_domain(dmclk_domain_sai2), 0);
    REG(STM32F7_RCC_DCKCFGR1_OFFSET) |= 2U << STM32F7_RCC_SAI1SEL_Pos;
    DMOD_TEST_EXPECT_EQ(dmclk_port_acquire_domain(dmclk_domain_sai1, 12288000, 3000, &first), -ENOTSUP);
    DMOD_TEST_EXPECT_EQ(dmclk_port_get_domain_frequency(dmclk_domain_sai1), 0U);
}

DMOD_TEST_STEP(clk48_reservation_and_concurrent_configuration)
{
    dmclk_frequency_t actual;
    DMOD_TEST_EXPECT_EQ(dmclk_port_acquire_domain(dmclk_domain_usb, 48000000, 0, &actual), 0);
    DMOD_TEST_EXPECT_EQ(dmclk_domain_begin_configuration(), -EBUSY);
    DMOD_TEST_EXPECT_EQ(dmclk_port_release_domain(dmclk_domain_usb), 0);
    DMOD_TEST_EXPECT_EQ(dmclk_domain_begin_configuration(), 0);
    DMOD_TEST_EXPECT_EQ(dmclk_port_acquire_domain(dmclk_domain_usb, 48000000, 0, &actual), -EBUSY);
    DMOD_TEST_EXPECT_EQ(dmclk_port_release_domain(dmclk_domain_usb), -EBUSY);
    dmclk_domain_end_configuration();
}

DMOD_TEST_STEP(fractional_readback_must_not_satisfy_exact_request)
{
    dmclk_frequency_t actual;
    DMOD_TEST_EXPECT_EQ(dmclk_port_acquire_domain(dmclk_domain_sai1, 12288000, 3000, &actual), 0);
    DMOD_TEST_EXPECT_EQ(dmclk_port_acquire_domain(dmclk_domain_sai1, actual, 0, &actual), -EBUSY);
}

DMOD_TEST_STEP(active_peripheral_and_busy_plls_are_not_disturbed)
{
    REG(0) |= STM32F7_RCC_PLLSAION | STM32F7_RCC_PLLSAIRDY;
    REG(0x44) |= 1U << 23; /* SAI2 gate enabled */
    uint32_t before[sizeof(rcc) / sizeof(rcc[0])];
    memcpy(before, rcc, sizeof(rcc));
    dmclk_frequency_t actual = 123;
    DMOD_TEST_EXPECT_EQ(dmclk_port_acquire_domain(dmclk_domain_sai2, 12288000, 3000, &actual), -EBUSY);
    DMOD_TEST_EXPECT_EQ(registers_differ(before), 0);
    REG(0x44) &= ~(1U << 23);
    DMOD_TEST_EXPECT_EQ(dmclk_port_acquire_domain(dmclk_domain_sai2, 12288000, 3000, &actual), 0);
    REG(0x44) |= 1U << 23;
    memcpy(before, rcc, sizeof(rcc));
    DMOD_TEST_EXPECT_EQ(dmclk_port_release_domain(dmclk_domain_sai2), -EBUSY);
    DMOD_TEST_EXPECT_EQ(registers_differ(before), 0);
    REG(0x44) &= ~(1U << 23);
    DMOD_TEST_EXPECT_EQ(dmclk_port_release_domain(dmclk_domain_sai2), 0);
    REG(0) |= STM32F7_RCC_PLLI2SON | STM32F7_RCC_PLLI2SRDY;
    REG(0x84) = REG(0x88);
    memcpy(before, rcc, sizeof(rcc));
    DMOD_TEST_EXPECT_EQ(dmclk_port_acquire_domain(dmclk_domain_sai2, 12288000, 3000, &actual), -EBUSY);
    DMOD_TEST_EXPECT_EQ(registers_differ(before), 0);
}

DMOD_TEST_STEP(missing_input_and_readback_follow_hardware)
{
    dmclk_frequency_t actual = 123;
    REG(0) &= ~RCC_CR_HSERDY;
    DMOD_TEST_EXPECT_EQ(dmclk_port_acquire_domain(dmclk_domain_sai1, 12000000, 0, &actual), -ERANGE);
    DMOD_TEST_EXPECT_EQ(actual, 123U);
    REG(0) |= RCC_CR_HSERDY;
    DMOD_TEST_EXPECT_EQ(dmclk_port_acquire_domain(dmclk_domain_sai1, 12000000, 0, &actual), 0);
    REG(0x88) = (REG(0x88) & ~RCC_PLLCFGR_PLLQ_Msk) | (15U << 24);
    DMOD_TEST_EXPECT_NE(dmclk_port_get_domain_frequency(dmclk_domain_sai1), actual);
    REG(0) &= ~STM32F7_RCC_PLLSAIRDY;
    DMOD_TEST_EXPECT_EQ(dmclk_port_get_domain_frequency(dmclk_domain_sai1), 0U);
}

/* Exhaustive reference search, deliberately different from the production
 * solver's adjacent-N search. Exercise HSE/HSI and non-integral input clocks. */
DMOD_TEST_STEP(sai_solver_matches_exhaustive_search)
{
    const uint32_t sources[] = {25000000, 16000000, 25000000};
    const uint32_t divisors[] = {25, 9, 19};
    const uint32_t targets[] = {12288000, 11289600, 49152000, 1, 216000000, UINT32_MAX};
    for (unsigned s = 0; s < 3; ++s) {
        for (unsigned t = 0; t < sizeof(targets) / sizeof(targets[0]); ++t) {
            stm32f7_sai_config_t config;
            DMOD_TEST_EXPECT_EQ(stm32f7_sai_solve(sources[s], divisors[s], targets[t], UINT64_MAX, &config), 0);
            uint32_t best_den = divisors[s] * config.q * config.div;
            uint64_t num = (uint64_t)sources[s] * config.n;
            uint64_t goal = (uint64_t)targets[t] * best_den;
            uint64_t best_error = num > goal ? num - goal : goal - num;
            int optimal = 1;
            for (unsigned n = 50; n <= 432; ++n) {
                uint64_t numerator = (uint64_t)sources[s] * n;
                if (numerator < 100000000ULL * divisors[s] || numerator > 432000000ULL * divisors[s]) { continue; }
                for (unsigned q = 2; q <= 15; ++q) {
                    for (unsigned d = 1; d <= 32; ++d) {
                        uint32_t den = divisors[s] * q * d;
                        uint64_t desired = (uint64_t)targets[t] * den;
                        uint64_t error = numerator > desired ? numerator - desired : desired - numerator;
                        if (error * best_den < best_error * den) { optimal = 0; }
                    }
                }
            }
            DMOD_TEST_EXPECT_TRUE(optimal);
        }
    }
}
