#include "clock_fixture.h"
#include "clock.h"
#include "port/stm32_common_regs.h"
#include <assert.h>
#include <string.h>

/* Register model for the shared STM32 PLLI2S Q/DIVQ IP, not a family port. */
static uint32_t registers[(RCC_DCKCFGR1_OFFSET + sizeof(uint32_t)) / sizeof(uint32_t)];
static uint32_t saved[sizeof(registers) / sizeof(*registers)];
static bool fail_lock, fail_stop, concurrent_call;
static unsigned critical_depth, elapsed_ms;
static int concurrent_result;
static stm32_clock_config_t config;
#define REG(offset) registers[(offset) / sizeof(uint32_t)]

void clock_fixture_init(bool sai_supported)
{
    memset(registers, 0, sizeof(registers));
    REG(RCC_CR_OFFSET) = RCC_CR_HSIRDY;
    REG(RCC_PLLCFGR_OFFSET) = 16;
    REG(RCC_PLLI2SCFGR_OFFSET) = 0x20003000;
    REG(RCC_PLLSAICFGR_OFFSET) = 0x76543210;
    REG(RCC_DCKCFGR1_OFFSET) = 0x00320103;
    config = (stm32_clock_config_t){.rcc_base = (uintptr_t)registers, .sai_supported = sai_supported};
    stm32_clock_init(&config);
    clock_fixture_snapshot();
}

void clock_fixture_snapshot(void)
{
    memcpy(saved, registers, sizeof(saved));
}

bool clock_fixture_unchanged(void)
{
    return memcmp(saved, registers, sizeof(saved)) == 0;
}

void clock_fixture_fault(bool lock_timeout, bool stop_timeout)
{
    fail_lock = lock_timeout;
    fail_stop = stop_timeout;
}

void clock_fixture_source(bool ready, bool unknown_hse)
{
    REG(RCC_PLLCFGR_OFFSET) = 16 | (unknown_hse ? RCC_PLLCFGR_PLLSRC : 0);
    REG(RCC_CR_OFFSET) = ready ? (unknown_hse ? RCC_CR_HSERDY : RCC_CR_HSIRDY) : 0;
}

void clock_fixture_external_owner(bool pll, bool peripheral)
{
    REG(RCC_CR_OFFSET) = RCC_CR_HSIRDY | (pll ? RCC_CR_PLLI2SON : 0);
    REG(RCC_APB2ENR_OFFSET) = peripheral ? RCC_APB2ENR_SAI2EN : 0;
}

void clock_fixture_gate_sai(bool enabled)
{
    if (enabled)
        REG(RCC_APB2ENR_OFFSET) |= RCC_APB2ENR_SAI2EN;
    else
        REG(RCC_APB2ENR_OFFSET) &= ~RCC_APB2ENR_SAI2EN;
}

void clock_fixture_concurrent_call(void)
{
    concurrent_call = true;
}

int clock_fixture_concurrent_result(void)
{
    return concurrent_result;
}

int clock_fixture_deinit(void)
{
    return stm32_clock_deinit();
}

unsigned clock_fixture_elapsed_ms(void)
{
    return elapsed_ms;
}

void Dmod_EnterCritical(void)
{
    assert(critical_depth++ == 0);
}

void Dmod_ExitCritical(void)
{
    assert(critical_depth-- == 1);
}

Dmod_Timestamp_t Dmod_GetUptime(void)
{
    assert(critical_depth == 0); /* Hardware waits must leave IRQs enabled. */
    if (REG(RCC_CR_OFFSET) & RCC_CR_PLLI2SON)
    {
        if (!fail_lock)
            REG(RCC_CR_OFFSET) |= RCC_CR_PLLI2SRDY;
    }
    else if (fail_stop)
        REG(RCC_CR_OFFSET) |= RCC_CR_PLLI2SRDY; /* Late lock / stop stuck. */
    else
        REG(RCC_CR_OFFSET) &= ~RCC_CR_PLLI2SRDY;
    if (concurrent_call)
    {
        concurrent_call = false;
        concurrent_result = dmclk_port_configure_internal(16000000, 0);
    }
    return elapsed_ms++;
}
