#include "sai_clock.h"
#include "port/stm32f7_regs.h"
#include "../stm32_common/stm32_common.h"
#include <errno.h>

/* Reuse a compatible running PLL or configure an idle auxiliary PLL. Never
 * stop/retune another consumer's source (in particular PLLSAI driving LTDC).
 * SAI mux changes follow HAL_RCCEx_PeriphCLKConfig, with the extra requirement
 * that the affected SAI peripheral clock gate is off to avoid clock glitches.
 * External SAI_CKIN is not measurable here and remains unsupported.
 */
typedef struct {
    uint32_t domains;
    uint32_t saved_config;
    uint32_t saved_div;
    int owned;
} sai_pll_state_t;

static sai_pll_state_t state[2];
static unsigned domain_source[2];
static unsigned saved_source[2];

static volatile uint32_t *reg(uintptr_t base, uint32_t offset)
{
    return (volatile uint32_t *)(base + offset);
}

static unsigned source_index(uintptr_t base, dmclk_domain_t domain)
{
    unsigned shift = domain == dmclk_domain_sai1 ? STM32F7_RCC_SAI1SEL_Pos : STM32F7_RCC_SAI2SEL_Pos;
    return (*reg(base, STM32F7_RCC_DCKCFGR1_OFFSET) >> shift) & 3U;
}

static uint32_t source_frequency(uintptr_t base, uint32_t hse)
{
    RCC_TypeDef *rcc = (RCC_TypeDef *)base;
    if (rcc->PLLCFGR & RCC_PLLCFGR_PLLSRC) {
        return (rcc->CR & RCC_CR_HSERDY) ? hse : 0;
    }
    return (rcc->CR & RCC_CR_HSIRDY) ? HSI_VALUE : 0;
}

static uint32_t enable_bit(unsigned source)
{
    return source == 0 ? STM32F7_RCC_PLLSAION : STM32F7_RCC_PLLI2SON;
}

static uint32_t config_offset(unsigned source)
{
    return source == 0 ? STM32F7_RCC_PLLSAICFGR_OFFSET : STM32F7_RCC_PLLI2SCFGR_OFFSET;
}

static unsigned div_shift(unsigned source)
{
    return source == 0 ? 8U : 0U;
}

/* Work with the rational frequency until the final readback: truncating the
 * oscillator / M first can falsely accept an exact-frequency request. */
static int within_tolerance(uint64_t numerator, uint32_t denominator,
                            dmclk_frequency_t target, dmclk_frequency_t tolerance)
{
    uint64_t desired = target * denominator;
    uint64_t error = numerator > desired ? numerator - desired : desired - numerator;
    return error / denominator < tolerance ||
           (error / denominator == tolerance && error % denominator == 0);
}

int stm32f7_sai_solve(uint32_t source, uint32_t m, dmclk_frequency_t target,
                     dmclk_frequency_t tolerance, stm32f7_sai_config_t *config)
{
    if (!config || !target) {
        return -EINVAL;
    }
    if (target > UINT32_MAX || m < 2 || m > 63 ||
        source < 1000000ULL * m || source > 2000000ULL * m) {
        return -ERANGE;
    }
    uint64_t best_error = UINT64_MAX;
    uint32_t best_den = 1;
    stm32f7_sai_config_t best = {0};
    uint32_t min_n = (uint32_t)((100000000ULL * m + source - 1) / source);
    uint32_t max_n = (uint32_t)(432000000ULL * m / source);
    if (min_n < 50) { min_n = 50; }
    if (max_n > 432) { max_n = 432; }
    for (uint32_t q = 2; q <= 15; ++q) {
        for (uint32_t div = 1; div <= 32; ++div) {
            uint32_t den = m * q * div;
            uint64_t ideal = target * den / source;
            for (unsigned adjacent = 0; adjacent < 2; ++adjacent) {
                uint64_t n = ideal + adjacent;
                if (n < min_n) { n = min_n; }
                if (n > max_n) { n = max_n; }
                uint64_t numerator = (uint64_t)source * n;
                if (numerator < 100000000ULL * m || numerator > 432000000ULL * m ||
                    !within_tolerance(numerator, den, target, tolerance)) {
                    continue;
                }
                uint64_t desired = target * den;
                uint64_t error = numerator > desired ? numerator - desired : desired - numerator;
                if (best.n == 0 || error * best_den < best_error * den) {
                    best_error = error;
                    best_den = den;
                    best.n = (uint32_t)n;
                    best.q = q;
                    best.div = div;
                    best.frequency = (numerator + den / 2) / den;
                }
            }
        }
    }
    if (best.n == 0) { return -ERANGE; }
    *config = best;
    return 0;
}

static int read_frequency(uintptr_t base, uint32_t hse, unsigned source,
                           uint64_t *numerator, uint32_t *denominator)
{
    RCC_TypeDef *rcc = (RCC_TypeDef *)base;
    uint32_t enabled = enable_bit(source);
    if ((rcc->CR & (enabled | (enabled << 1))) != (enabled | (enabled << 1))) {
        return 0;
    }
    uint32_t input = source_frequency(base, hse);
    uint32_t m = rcc->PLLCFGR & RCC_PLLCFGR_PLLM_Msk;
    uint32_t cfg = *reg(base, config_offset(source));
    uint32_t n = (cfg & RCC_PLLCFGR_PLLN_Msk) >> RCC_PLLCFGR_PLLN_Pos;
    uint32_t q = (cfg & RCC_PLLCFGR_PLLQ_Msk) >> RCC_PLLCFGR_PLLQ_Pos;
    uint32_t div = ((*reg(base, STM32F7_RCC_DCKCFGR1_OFFSET) >> div_shift(source)) & 31U) + 1;
    if (m < 2 || n < 50 || n > 432 || q < 2 ||
        input < 1000000ULL * m || input > 2000000ULL * m) {
        return 0;
    }
    *numerator = (uint64_t)input * n;
    *denominator = m * q * div;
    return *numerator >= 100000000ULL * m && *numerator <= 432000000ULL * m;
}

dmclk_frequency_t stm32f7_sai_frequency(uintptr_t base, uint32_t hse, dmclk_domain_t domain)
{
    if (domain != dmclk_domain_sai1 && domain != dmclk_domain_sai2) { return 0; }
    unsigned source = source_index(base, domain);
    uint64_t numerator;
    uint32_t denominator;
    if (source > 1 || !read_frequency(base, hse, source, &numerator, &denominator)) { return 0; }
    return (numerator + denominator / 2) / denominator;
}

static int stop_pll(uintptr_t base, unsigned source)
{
    RCC_TypeDef *rcc = (RCC_TypeDef *)base;
    uint32_t enabled = enable_bit(source);
    rcc->CR &= ~enabled;
    return stm32_wait_clock_stopped(base, enabled << 1, STM32F7_SAI_PLL_TIMEOUT) == 0 ? 0 : -ETIMEDOUT;
}

static void restore_config(uintptr_t base, unsigned source)
{
    *reg(base, config_offset(source)) = state[source].saved_config;
    volatile uint32_t *dck = reg(base, STM32F7_RCC_DCKCFGR1_OFFSET);
    uint32_t mask = 31U << div_shift(source);
    *dck = (*dck & ~mask) | state[source].saved_div;
    state[source].owned = 0;
}

static unsigned mux_shift(dmclk_domain_t domain)
{
    return domain == dmclk_domain_sai1 ? STM32F7_RCC_SAI1SEL_Pos : STM32F7_RCC_SAI2SEL_Pos;
}

static int gate_enabled(uintptr_t base, dmclk_domain_t domain)
{
    /* RCC_APB2ENR.SAI1EN/SAI2EN. Acquiring precedes enabling the peripheral. */
    unsigned bit = domain == dmclk_domain_sai1 ? 22U : 23U;
    return (*reg(base, 0x44U) & (1U << bit)) != 0;
}

static void select_source(uintptr_t base, dmclk_domain_t domain, unsigned source)
{
    unsigned shift = mux_shift(domain);
    volatile uint32_t *dck = reg(base, STM32F7_RCC_DCKCFGR1_OFFSET);
    *dck = (*dck & ~(3U << shift)) | (source << shift);
}

static int matches_source(uintptr_t base, uint32_t hse, unsigned source,
                          dmclk_frequency_t target, dmclk_frequency_t tolerance,
                          dmclk_frequency_t *frequency)
{
    uint64_t numerator;
    uint32_t denominator;
    if (!read_frequency(base, hse, source, &numerator, &denominator) ||
        !within_tolerance(numerator, denominator, target, tolerance)) { return 0; }
    *frequency = (numerator + denominator / 2) / denominator;
    return 1;
}

int stm32f7_sai_acquire(uintptr_t base, uint32_t hse, dmclk_domain_t domain,
                      dmclk_frequency_t target, dmclk_frequency_t tolerance,
                      dmclk_frequency_t *actual)
{
    unsigned original = source_index(base, domain);
    if (original > 1) { return -ENOTSUP; }
    if (target > UINT32_MAX) { return -ERANGE; }
    unsigned index = domain == dmclk_domain_sai1 ? 0 : 1;
    unsigned source = original;
    RCC_TypeDef *rcc = (RCC_TypeDef *)base;
    dmclk_frequency_t frequency;
    int matches = matches_source(base, hse, source, target, tolerance, &frequency);
    int held = (state[0].domains | state[1].domains) & (1U << index);
    if (held) {
        if (!matches) { return -EBUSY; }
        *actual = frequency;
        return 0;
    }
    if (!matches && (rcc->CR & (enable_bit(source) * 3U))) {
        /* The selected PLL belongs to another consumer. Try the other one
         * without changing the selected clock of any other domain. */
        if (gate_enabled(base, domain)) { return -EBUSY; }
        source ^= 1U;
        matches = matches_source(base, hse, source, target, tolerance, &frequency);
        if (!matches && (rcc->CR & (enable_bit(source) * 3U))) { return -EBUSY; }
    }
    if (!matches) {
        if (state[source].domains || gate_enabled(base, domain)) { return -EBUSY; }
        stm32f7_sai_config_t config;
        int rc = stm32f7_sai_solve(source_frequency(base, hse),
                                 rcc->PLLCFGR & RCC_PLLCFGR_PLLM_Msk,
                                 target, tolerance, &config);
        if (rc != 0) { return rc; }
        /* Recover a failed start whose ready bit took too long to clear. */
        if (state[source].owned) { restore_config(base, source); }
        state[source].saved_config = *reg(base, config_offset(source));
        uint32_t shift = div_shift(source);
        volatile uint32_t *dck = reg(base, STM32F7_RCC_DCKCFGR1_OFFSET);
        state[source].saved_div = *dck & (31U << shift);
        state[source].owned = 1;
        *reg(base, config_offset(source)) =
            (state[source].saved_config & ~(RCC_PLLCFGR_PLLN_Msk | RCC_PLLCFGR_PLLQ_Msk)) |
            (config.n << RCC_PLLCFGR_PLLN_Pos) | (config.q << RCC_PLLCFGR_PLLQ_Pos);
        *dck = (*dck & ~(31U << shift)) | ((config.div - 1) << shift);
        select_source(base, domain, source);
        uint32_t enabled = enable_bit(source);
        rcc->CR |= enabled;
        if (stm32_wait_clock_ready(base, enabled << 1, STM32F7_SAI_PLL_TIMEOUT) != 0) {
            select_source(base, domain, original);
            if (stop_pll(base, source) == 0) { restore_config(base, source); }
            return -ETIMEDOUT;
        }
        frequency = stm32f7_sai_frequency(base, hse, domain);
    } else if (source != original) {
        select_source(base, domain, source);
    }
    saved_source[index] = original;
    state[source].domains |= 1U << index;
    domain_source[index] = source;
    *actual = frequency;
    return 0;
}

int stm32f7_sai_release(uintptr_t base, dmclk_domain_t domain)
{
    unsigned index = domain == dmclk_domain_sai1 ? 0 : 1;
    unsigned source = domain_source[index];
    uint32_t remaining = state[source].domains & ~(1U << index);
    if ((saved_source[index] != source || (!remaining && state[source].owned)) &&
        gate_enabled(base, domain)) { return -EBUSY; }
    if (!remaining && state[source].owned) {
        int rc = stop_pll(base, source);
        if (rc != 0) {
            /* Keep the lease and resume the clock; the caller can retry. */
            ((RCC_TypeDef *)base)->CR |= enable_bit(source);
            return rc;
        }
        restore_config(base, source);
    }
    if (saved_source[index] != source) { select_source(base, domain, saved_source[index]); }
    state[source].domains = remaining;
    return 0;
}
