#include "sai_clock.h"
#include "port/stm32_common_regs.h"
#include "port/stm32f7_regs.h"
#include <errno.h>
#include <limits.h>

#ifndef REG
#define REG(offset) (*(volatile uint32_t *)(STM32F7_RCC_BASE + (offset)))
#endif
#ifndef DMCLK_CLOCK_POLL
#define DMCLK_CLOCK_POLL() ((void)0)
#endif
#define PLLI2S_ON (1UL << 26)
#define PLLI2S_READY (1UL << 27)
#define SAI_ENABLES ((1UL << 22) | (1UL << 23))
#define PLLI2S_FIELDS ((0x1FFUL << 6) | (0xFUL << 24))
#ifndef WAIT_LOOPS
#define WAIT_LOOPS 1000000U
#endif

static struct
{
    bool busy;
    unsigned refs[2];
    uint32_t saved_mux[2];
    uint32_t saved_pll;
    uint32_t saved_div;
} clock_state;

static int domain_index(dmclk_domain_t domain)
{
    if (domain == dmclk_domain_sai1) return 0;
    if (domain == dmclk_domain_sai2) return 1;
    return -1;
}

static int begin(bool main_clock)
{
    int result = 0;
    Dmod_EnterCritical();
    if (clock_state.busy || (main_clock && (clock_state.refs[0] || clock_state.refs[1])))
        result = -EBUSY;
    else clock_state.busy = true;
    Dmod_ExitCritical();
    return result;
}

int stm32f7_clock_begin(void) { return begin(true); }
void stm32f7_clock_end(void)
{
    Dmod_EnterCritical();
    clock_state.busy = false;
    Dmod_ExitCritical();
}

static int wait_plli2s(bool ready)
{
    for (unsigned i = 0; i < WAIT_LOOPS; i++)
    {
        DMCLK_CLOCK_POLL();
        if (((REG(0x00) & PLLI2S_READY) != 0) == ready) return 0;
    }
    return -ETIMEDOUT;
}

static uint32_t pll_frequency(void)
{
    uint32_t m = REG(0x04) & 63U;
    uint32_t n = (REG(0x84) >> 6) & 511U;
    uint32_t q = (REG(0x84) >> 24) & 15U;
    uint32_t div = (REG(0x8C) & 31U) + 1U;
    if (!(REG(0x00) & PLLI2S_READY) || m < 2 || n < 50 || q < 2) return 0;
    uint64_t denominator = (uint64_t)m * q * div;
    return ((uint64_t)stm32f7_pll_source_frequency() * n + denominator / 2) / denominator;
}

dmclk_frequency_t stm32f7_sai_frequency(dmclk_domain_t domain)
{
    int index = domain_index(domain);
    if (index < 0 || ((REG(0x8C) >> (20 + index * 2)) & 3U) != 1U) return 0;
    return pll_frequency();
}

static void restore_pll(void)
{
    REG(0x84) = clock_state.saved_pll;
    REG(0x8C) = (REG(0x8C) & ~31UL) | clock_state.saved_div;
}

static int program_pll(const stm32f7_sai_setting_t *setting)
{
    clock_state.saved_pll = REG(0x84);
    clock_state.saved_div = REG(0x8C) & 31U;
    REG(0x84) = (REG(0x84) & ~PLLI2S_FIELDS) | (setting->n << 6) | (setting->q << 24);
    REG(0x8C) = (REG(0x8C) & ~31UL) | (setting->divq - 1);
    REG(0x00) |= PLLI2S_ON;
    int result = wait_plli2s(true);
    if (result)
    {
        REG(0x00) &= ~PLLI2S_ON;
        if (wait_plli2s(false) == 0) restore_pll();
    }
    return result;
}

static bool frequency_matches(uint32_t target, uint32_t tolerance)
{
    uint32_t m = REG(0x04) & 63U, q = (REG(0x84) >> 24) & 15U;
    uint32_t n = (REG(0x84) >> 6) & 511U, div = (REG(0x8C) & 31U) + 1;
    if (!m || !q || !(REG(0x00) & PLLI2S_READY)) return false;
    uint64_t denominator = (uint64_t)m * q * div;
    uint64_t numerator = (uint64_t)stm32f7_pll_source_frequency() * n;
    uint64_t wanted = (uint64_t)target * denominator;
    uint64_t error = numerator > wanted ? numerator - wanted : wanted - numerator;
    return error <= (uint64_t)tolerance * denominator;
}

static int acquire(int index, uint32_t target, uint32_t tolerance, dmclk_frequency_t *actual)
{
    if (!clock_state.refs[index] && (REG(0x44) & (1UL << (22 + index)))) return -EBUSY;
    if (clock_state.refs[index] == UINT_MAX) return -EOVERFLOW;
    uint32_t frequency = pll_frequency();
    if (clock_state.refs[0] || clock_state.refs[1])
    {
        if (!frequency || !frequency_matches(target, tolerance)) return -EBUSY;
    }
    else
    {
        /* An external PLLI2S owner (I2S/SPDIF/SAI) must not be reconfigured. */
        if ((REG(0x00) & (PLLI2S_ON | PLLI2S_READY)) || (REG(0x44) & SAI_ENABLES))
            return -EBUSY;
        uint32_t ready = (REG(0x04) & RCC_PLLCFGR_PLLSRC) ? RCC_CR_HSERDY : RCC_CR_HSIRDY;
        if (!(REG(0x00) & ready)) return -ENODATA;
        stm32f7_sai_setting_t setting;
        int result = stm32f7_sai_solve(stm32f7_pll_source_frequency(), REG(0x04) & 63U,
                                      target, tolerance, &setting);
        if (result) return result;
        result = program_pll(&setting);
        if (result) return result;
        frequency = pll_frequency();
    }
    uint32_t shift = 20 + index * 2;
    if (!clock_state.refs[index])
    {
        clock_state.saved_mux[index] = REG(0x8C) & (3UL << shift);
        REG(0x8C) = (REG(0x8C) & ~(3UL << shift)) | (1UL << shift);
    }
    clock_state.refs[index]++;
    *actual = frequency;
    return 0;
}

dmod_dmclk_port_api_declaration(1.0, int, _sai_acquire, ( dmclk_domain_t domain, dmclk_frequency_t target, dmclk_frequency_t tolerance, dmclk_frequency_t *actual ) )
{
    int index = domain_index(domain);
    if (index < 0 || !actual || !target || target > UINT32_MAX || tolerance > UINT32_MAX)
        return -EINVAL;
    int result = begin(false);
    if (result) return result;
    result = acquire(index, target, tolerance, actual);
    stm32f7_clock_end();
    return result;
}

dmod_dmclk_port_api_declaration(1.0, int, _sai_release, ( dmclk_domain_t domain ) )
{
    int index = domain_index(domain);
    if (index < 0) return -EINVAL;
    int result = begin(false);
    if (result) return result;
    if (!clock_state.refs[index]) result = -EINVAL;
    else if ((REG(0x44) & (1UL << (22 + index))) && clock_state.refs[index] == 1)
        result = -EBUSY; /* Consumer must stop/gate its SAI before releasing. */
    else if (--clock_state.refs[index] == 0)
    {
        uint32_t shift = 20 + index * 2;
        REG(0x8C) = (REG(0x8C) & ~(3UL << shift)) | clock_state.saved_mux[index];
        if (!clock_state.refs[0] && !clock_state.refs[1])
        {
            REG(0x00) &= ~PLLI2S_ON;
            result = wait_plli2s(false);
            if (!result) restore_pll();
            else
            {
                /* Keep the reservation retryable if hardware cannot stop. */
                clock_state.refs[index] = 1;
                REG(0x8C) = (REG(0x8C) & ~(3UL << shift)) | (1UL << shift);
                REG(0x00) |= PLLI2S_ON;
            }
        }
    }
    stm32f7_clock_end();
    return result;
}
