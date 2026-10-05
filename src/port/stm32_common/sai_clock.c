#include "sai_clock.h"
#include "clock.h"
#include "port/stm32_common_regs.h"
#include <errno.h>
#include <limits.h>

#define PLLI2S_TIMEOUT_MS 100U
#define PLLI2S_FIELDS (RCC_PLLI2SCFGR_PLLI2SN_Msk | RCC_PLLI2SCFGR_PLLI2SQ_Msk)

static struct
{
    bool busy;
    bool recovery_pending;
    uint8_t refs[2];
    uint32_t saved_mux[2];
    uint32_t saved_pll;
    uint32_t saved_div;
} clock_state;

static const uint32_t mux_shift[] = {RCC_DCKCFGR1_SAI1SEL_Pos, RCC_DCKCFGR1_SAI2SEL_Pos};
static const uint32_t sai_enable[] = {RCC_APB2ENR_SAI1EN, RCC_APB2ENR_SAI2EN};

static int domain_index(dmclk_domain_t domain)
{
    if (domain == dmclk_domain_sai1)
        return 0;
    if (domain == dmclk_domain_sai2)
        return 1;
    return -1;
}

static int begin(bool main_clock)
{
    int result = 0;
    Dmod_EnterCritical();
    bool reserved = clock_state.refs[0] || clock_state.refs[1] || clock_state.recovery_pending;
    if (clock_state.busy || (main_clock && reserved))
        result = -EBUSY;
    else
        clock_state.busy = true;
    Dmod_ExitCritical();
    return result;
}

int stm32_clock_begin(void)
{
    return begin(true);
}

void stm32_clock_end(void)
{
    Dmod_EnterCritical();
    clock_state.busy = false;
    Dmod_ExitCritical();
}

static int wait_plli2s(bool ready)
{
    Dmod_Timestamp_t start = Dmod_GetUptime();
    while (((STM32_RCC(RCC_CR_OFFSET) & RCC_CR_PLLI2SRDY) != 0) != ready)
    {
        if (Dmod_GetUptime() - start >= PLLI2S_TIMEOUT_MS)
            return -ETIMEDOUT;
    }
    return 0;
}

static uint32_t pll_frequency(void)
{
    uint32_t m = STM32_RCC(RCC_PLLCFGR_OFFSET) & RCC_PLLCFGR_PLLM_Msk;
    uint32_t pll = STM32_RCC(RCC_PLLI2SCFGR_OFFSET);
    uint32_t n = (pll & RCC_PLLI2SCFGR_PLLI2SN_Msk) >> RCC_PLLI2SCFGR_PLLI2SN_Pos;
    uint32_t q = (pll & RCC_PLLI2SCFGR_PLLI2SQ_Msk) >> RCC_PLLI2SCFGR_PLLI2SQ_Pos;
    uint32_t div = (STM32_RCC(RCC_DCKCFGR1_OFFSET) & RCC_DCKCFGR1_PLLI2SDIVQ_Msk) + 1U;
    if (!(STM32_RCC(RCC_CR_OFFSET) & RCC_CR_PLLI2SRDY) || m < 2 || n < 50 || q < 2)
        return 0;
    uint64_t denominator = (uint64_t)m * q * div;
    return ((uint64_t)stm32_pll_source_frequency() * n + denominator / 2) / denominator;
}

dmclk_frequency_t stm32_sai_frequency(dmclk_domain_t domain)
{
    int index = domain_index(domain);
    if (index < 0)
        return 0;
    uint32_t mux = (STM32_RCC(RCC_DCKCFGR1_OFFSET) >> mux_shift[index]) & RCC_DCKCFGR1_SAISEL_Msk;
    return mux == RCC_DCKCFGR1_SAISEL_PLLI2S ? pll_frequency() : 0;
}

static void restore_pll(void)
{
    STM32_RCC(RCC_PLLI2SCFGR_OFFSET) = clock_state.saved_pll;
    STM32_RCC(RCC_DCKCFGR1_OFFSET) =
        (STM32_RCC(RCC_DCKCFGR1_OFFSET) & ~RCC_DCKCFGR1_PLLI2SDIVQ_Msk) | clock_state.saved_div;
    clock_state.recovery_pending = false;
}

static int stop_and_restore(void)
{
    STM32_RCC(RCC_CR_OFFSET) &= ~RCC_CR_PLLI2SON;
    int result = wait_plli2s(false);
    if (result == 0)
        restore_pll();
    return result;
}

static int program_pll(const stm32_sai_setting_t *setting)
{
    clock_state.saved_pll = STM32_RCC(RCC_PLLI2SCFGR_OFFSET);
    clock_state.saved_div = STM32_RCC(RCC_DCKCFGR1_OFFSET) & RCC_DCKCFGR1_PLLI2SDIVQ_Msk;
    uint32_t value = (setting->n << RCC_PLLI2SCFGR_PLLI2SN_Pos)
                   | (setting->q << RCC_PLLI2SCFGR_PLLI2SQ_Pos);
    STM32_RCC(RCC_PLLI2SCFGR_OFFSET) = (clock_state.saved_pll & ~PLLI2S_FIELDS) | value;
    STM32_RCC(RCC_DCKCFGR1_OFFSET) =
        (STM32_RCC(RCC_DCKCFGR1_OFFSET) & ~RCC_DCKCFGR1_PLLI2SDIVQ_Msk) | (setting->divq - 1);
    STM32_RCC(RCC_CR_OFFSET) |= RCC_CR_PLLI2SON;
    int result = wait_plli2s(true);
    if (result != 0)
    {
        /* A late lock may keep RDY high after ON is cleared. Do not rewrite
         * an active PLL. Retain the original snapshot for the next acquire. */
        clock_state.recovery_pending = true;
        (void)stop_and_restore();
    }
    return result;
}

static bool frequency_matches(uint32_t target, uint32_t tolerance)
{
    uint32_t m = STM32_RCC(RCC_PLLCFGR_OFFSET) & RCC_PLLCFGR_PLLM_Msk;
    uint32_t pll = STM32_RCC(RCC_PLLI2SCFGR_OFFSET);
    uint32_t n = (pll & RCC_PLLI2SCFGR_PLLI2SN_Msk) >> RCC_PLLI2SCFGR_PLLI2SN_Pos;
    uint32_t q = (pll & RCC_PLLI2SCFGR_PLLI2SQ_Msk) >> RCC_PLLI2SCFGR_PLLI2SQ_Pos;
    uint32_t div = (STM32_RCC(RCC_DCKCFGR1_OFFSET) & RCC_DCKCFGR1_PLLI2SDIVQ_Msk) + 1U;
    if (!m || !q || !(STM32_RCC(RCC_CR_OFFSET) & RCC_CR_PLLI2SRDY))
        return false;
    uint64_t denominator = (uint64_t)m * q * div;
    uint64_t numerator = (uint64_t)stm32_pll_source_frequency() * n;
    uint64_t wanted = (uint64_t)target * denominator;
    uint64_t error = numerator > wanted ? numerator - wanted : wanted - numerator;
    return error <= (uint64_t)tolerance * denominator;
}

static int start_clock(uint32_t target, uint32_t tolerance)
{
    if (clock_state.recovery_pending)
    {
        int result = stop_and_restore();
        if (result != 0)
            return result;
    }
    if ((STM32_RCC(RCC_CR_OFFSET) & (RCC_CR_PLLI2SON | RCC_CR_PLLI2SRDY)) ||
        (STM32_RCC(RCC_APB2ENR_OFFSET) & RCC_APB2ENR_SAIEN_Msk))
        return -EBUSY;
    uint32_t ready = (STM32_RCC(RCC_PLLCFGR_OFFSET) & RCC_PLLCFGR_PLLSRC)
        ? RCC_CR_HSERDY : RCC_CR_HSIRDY;
    uint32_t source = stm32_pll_source_frequency();
    if (!(STM32_RCC(RCC_CR_OFFSET) & ready) || !source)
        return -ENODATA;
    stm32_sai_setting_t setting;
    int result = stm32_sai_solve(source, STM32_RCC(RCC_PLLCFGR_OFFSET) & RCC_PLLCFGR_PLLM_Msk,
                                target, tolerance, &setting);
    return result != 0 ? result : program_pll(&setting);
}

static void set_mux(int index, uint32_t value)
{
    uint32_t mask = RCC_DCKCFGR1_SAISEL_Msk << mux_shift[index];
    STM32_RCC(RCC_DCKCFGR1_OFFSET) = (STM32_RCC(RCC_DCKCFGR1_OFFSET) & ~mask) | value;
}

static int acquire(int index, uint32_t target, uint32_t tolerance, dmclk_frequency_t *actual)
{
    if (!clock_state.refs[index] && (STM32_RCC(RCC_APB2ENR_OFFSET) & sai_enable[index]))
        return -EBUSY;
    if (clock_state.refs[index] == UINT8_MAX)
        return -EOVERFLOW;
    if (clock_state.refs[0] || clock_state.refs[1])
    {
        if (!pll_frequency() || !frequency_matches(target, tolerance))
            return -EBUSY;
    }
    else
    {
        int result = start_clock(target, tolerance);
        if (result != 0)
            return result;
    }
    if (!clock_state.refs[index])
    {
        clock_state.saved_mux[index] = STM32_RCC(RCC_DCKCFGR1_OFFSET)
            & (RCC_DCKCFGR1_SAISEL_Msk << mux_shift[index]);
        set_mux(index, RCC_DCKCFGR1_SAISEL_PLLI2S << mux_shift[index]);
    }
    clock_state.refs[index]++;
    *actual = pll_frequency();
    return 0;
}

dmod_dmclk_port_api_declaration(1.0, int, _sai_acquire,
    (dmclk_domain_t domain, dmclk_frequency_t target, dmclk_frequency_t tolerance, dmclk_frequency_t *actual))
{
    if (!stm32_clock_config()->sai_supported)
        return -ENOTSUP;
    int index = domain_index(domain);
    if (index < 0 || !actual || !target || target > UINT32_MAX || tolerance > UINT32_MAX)
        return -EINVAL;
    int result = begin(false);
    if (result != 0)
        return result;
    result = acquire(index, target, tolerance, actual);
    stm32_clock_end();
    return result;
}

static int release(int index)
{
    if (!clock_state.refs[index])
        return -EINVAL;
    if (clock_state.refs[index] == 1 && (STM32_RCC(RCC_APB2ENR_OFFSET) & sai_enable[index]))
        return -EBUSY;
    if (--clock_state.refs[index] != 0)
        return 0;
    set_mux(index, clock_state.saved_mux[index]);
    if (clock_state.refs[0] || clock_state.refs[1])
        return 0;
    int result = stop_and_restore();
    if (result != 0)
    {
        /* Keep the reservation owned and retryable until the PLL stops. */
        clock_state.refs[index] = 1;
        set_mux(index, RCC_DCKCFGR1_SAISEL_PLLI2S << mux_shift[index]);
        STM32_RCC(RCC_CR_OFFSET) |= RCC_CR_PLLI2SON;
    }
    return result;
}

dmod_dmclk_port_api_declaration(1.0, int, _sai_release, (dmclk_domain_t domain))
{
    if (!stm32_clock_config()->sai_supported)
        return -ENOTSUP;
    int index = domain_index(domain);
    if (index < 0)
        return -EINVAL;
    int result = begin(false);
    if (result != 0)
        return result;
    result = release(index);
    stm32_clock_end();
    return result;
}
