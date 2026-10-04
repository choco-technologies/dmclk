#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "sai_clock.h"
static uint32_t registers[40];
static bool fail_ready;
static unsigned critical_depth;
void Dmod_EnterCritical(void) { assert(!critical_depth++); }
void Dmod_ExitCritical(void) { assert(critical_depth-- == 1); }
uint32_t stm32f7_pll_source_frequency(void) { return 25000000; }
static void poll(void)
{
    assert(!critical_depth); /* PLL waits must leave interrupts enabled. */
    if ((registers[0] & (1U << 26)) && !fail_ready) registers[0] |= 1U << 27;
    else registers[0] &= ~(1U << 27);
}
#define REG(offset) registers[(offset) / 4]
#define WAIT_LOOPS 8
#define DMCLK_CLOCK_POLL() poll()
#include "../../src/port/stm32f7/sai_clock.c"

static void solver(void)
{
    const uint32_t rates[] = {8000, 11025, 16000, 22050, 32000, 44100, 48000, 96000};
    for (unsigned i = 0; i < sizeof(rates) / sizeof(*rates); i++)
    {
        uint32_t target = 256 * rates[i];
        stm32f7_sai_setting_t s;
        assert(stm32f7_sai_solve(25000000, 25, target, target / 2000, &s) == 0);
        uint64_t numerator = 25000000ULL * s.n, denominator = 25ULL * s.q * s.divq;
        uint64_t wanted = target * denominator;
        uint64_t error = numerator > wanted ? numerator - wanted : wanted - numerator;
        assert(error <= (target / 2000) * denominator);
        assert(s.n >= 100 && s.n <= 432 && s.q >= 2 && s.q <= 15 && s.divq <= 32);
    }
    stm32f7_sai_setting_t s = {1, 2, 3, 4}, before = s;
    assert(stm32f7_sai_solve(25000000, 25, 12288000, 0, &s) == -ERANGE);
    assert(memcmp(&s, &before, sizeof(s)) == 0);
    assert(stm32f7_sai_solve(25000000, 1, 12288000, 10000, &s) == -EINVAL);
    assert(stm32f7_sai_solve(25000000, 25, 0, 10000, &s) == -EINVAL);
    assert(stm32f7_sai_solve(25000000, 25, 12288000, 10000, NULL) == -EINVAL);
    assert(stm32f7_sai_solve(16000000, 16, 11289600, 1000, &s) == 0);
}

static void ownership(void)
{
    registers[0] = RCC_CR_HSERDY;
    registers[1] = RCC_PLLCFGR_PLLSRC | 25;
    registers[0x84 / 4] = 0x20003000;
    registers[0x88 / 4] = 0x76543210;
    registers[0x8C / 4] = 0x00320103; /* Preserve LCD divider, other SAI mux. */
    uint32_t saved[40]; memcpy(saved, registers, sizeof(saved));
    dmclk_frequency_t actual = 999, shared = 0;
    assert(dmclk_port_sai_acquire(dmclk_domain_sai2, 12288000, 6144, &actual) == 0);
    assert(actual > 12281000 && actual < 12295000);
    assert(stm32f7_clock_begin() == -EBUSY);
    assert(dmclk_port_sai_acquire(dmclk_domain_sai2, actual, 0, &shared) == -EBUSY);
    assert(dmclk_port_sai_acquire(dmclk_domain_sai1, 12288000, 6144, &shared) == 0);
    assert(shared == actual && stm32f7_sai_frequency(dmclk_domain_sai1) == actual);
    assert(dmclk_port_sai_acquire(dmclk_domain_sai2, 11289600, 5644, &shared) == -EBUSY);
    assert(dmclk_port_sai_acquire(dmclk_domain_sai2, 12288000, 6144, &shared) == 0);
    assert(dmclk_port_sai_release(dmclk_domain_sai2) == 0);
    registers[0x44 / 4] |= 1U << 23;
    assert(dmclk_port_sai_release(dmclk_domain_sai2) == -EBUSY);
    registers[0x44 / 4] &= ~(1U << 23);
    assert(dmclk_port_sai_release(dmclk_domain_sai2) == 0);
    assert(registers[0] & PLLI2S_ON);
    assert(dmclk_port_sai_release(dmclk_domain_sai1) == 0);
    assert(memcmp(saved, registers, sizeof(saved)) == 0);
    assert(dmclk_port_sai_release(dmclk_domain_sai1) == -EINVAL);
    assert(stm32f7_clock_begin() == 0); stm32f7_clock_end();
    assert(dmclk_port_sai_acquire(dmclk_domain_usb, 12288000, 6144, &actual) == -EINVAL);
    fail_ready = true;
    assert(dmclk_port_sai_acquire(dmclk_domain_sai2, 12288000, 6144, &actual) == -ETIMEDOUT);
    assert(memcmp(saved, registers, sizeof(saved)) == 0);
    fail_ready = false;
    registers[0] |= PLLI2S_ON;
    assert(dmclk_port_sai_acquire(dmclk_domain_sai2, 12288000, 6144, &actual) == -EBUSY);
    registers[0] &= ~PLLI2S_ON;
    registers[0x44 / 4] |= 1U << 23;
    assert(dmclk_port_sai_acquire(dmclk_domain_sai2, 12288000, 6144, &actual) == -EBUSY);
}
int main(void) { solver(); ownership(); puts("SAI solver and clock ownership: PASS"); }
