#include "dmod.h"
#include "dmclk_port.h"
#include "port/stm32f7_regs.h"
#include <errno.h>
#include <stdbool.h>

#define RCC(offset) (*(volatile uint32_t *)(STM32F7_RCC_BASE + (offset)))

static int check_rate(uint32_t rate)
{
    uint32_t cfgr = RCC(RCC_PLLCFGR_OFFSET);
    uint32_t lcd = RCC(RCC_PLLSAICFGR_OFFSET);
    uint32_t dck = RCC(RCC_DCKCFGR1_OFFSET);
    uint32_t pll = RCC(RCC_PLLI2SCFGR_OFFSET);
    dmclk_frequency_t actual = 0, shared = 0;
    uint32_t target = rate * 256;
    int result = dmclk_port_sai_acquire(dmclk_domain_sai2, target, target / 2000, &actual);
    if (result)
        return result;
    bool valid = dmclk_port_get_domain_frequency(dmclk_domain_sai2) == actual;
    valid &= RCC(RCC_PLLCFGR_OFFSET) == cfgr && RCC(RCC_PLLSAICFGR_OFFSET) == lcd;
    /* Reconfiguration guards are tested against simulated RCC on the host.
     * Never attempt SYSCLK reconfiguration in a live-board negative test. */
    valid &= dmclk_port_sai_acquire(dmclk_domain_sai2, target, target / 2000, &shared) == 0;
    if (shared)
        valid &= dmclk_port_sai_release(dmclk_domain_sai2) == 0;
    uint32_t other = rate == 48000 ? 11289600 : 12288000;
    valid &= dmclk_port_sai_acquire(dmclk_domain_sai1, other, other / 2000, &shared) == -EBUSY;
    Dmod_Printf("SAI_CLOCK rate=%u kernel=%llu PLLI2SCFGR=%08lx DCKCFGR1=%08lx\n",
                rate, (unsigned long long)actual,
                (unsigned long)RCC(RCC_PLLI2SCFGR_OFFSET),
                (unsigned long)RCC(RCC_DCKCFGR1_OFFSET));
    valid &= dmclk_port_sai_release(dmclk_domain_sai2) == 0;
    valid &= RCC(RCC_PLLCFGR_OFFSET) == cfgr && RCC(RCC_PLLSAICFGR_OFFSET) == lcd;
    valid &= RCC(RCC_DCKCFGR1_OFFSET) == dck && RCC(RCC_PLLI2SCFGR_OFFSET) == pll;
    valid &= dmclk_port_sai_release(dmclk_domain_sai2) == -EINVAL;
    return valid ? 0 : -EIO;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    int result = check_rate(48000);
    if (!result)
        result = check_rate(44100);
    Dmod_Printf("SAI_CLOCK_HARDWARE %s result=%d\n", result ? "FAIL" : "PASS", result);
    return result;
}
