#include "dmod.h"
#include "dmclk_port.h"
#include <errno.h>
#define RCC(offset) (*(volatile uint32_t *)(0x40023800UL + (offset)))
static int check_rate(uint32_t rate)
{
    uint32_t cfgr = RCC(4), lcd = RCC(0x88), dck = RCC(0x8c), pll = RCC(0x84);
    dmclk_frequency_t actual = 0, shared = 0;
    uint32_t target = rate * 256;
    int result = dmclk_port_sai_acquire(dmclk_domain_sai2, target, target / 2000, &actual);
    if (result) return result;
    bool valid = dmclk_port_get_domain_frequency(dmclk_domain_sai2) == actual;
    valid &= RCC(4) == cfgr && RCC(0x88) == lcd;
    valid &= dmclk_port_configure_external(216000000, 1000, 25000000) == -EBUSY;
    valid &= dmclk_port_sai_acquire(dmclk_domain_sai2, target, target / 2000, &shared) == 0;
    if (shared) valid &= dmclk_port_sai_release(dmclk_domain_sai2) == 0;
    uint32_t other = rate == 48000 ? 11289600 : 12288000;
    valid &= dmclk_port_sai_acquire(dmclk_domain_sai1, other, other / 2000, &shared) == -EBUSY;
    Dmod_Printf("SAI_CLOCK rate=%u kernel=%llu PLLI2SCFGR=%08lx DCKCFGR1=%08lx\n",
                rate, (unsigned long long)actual, (unsigned long)RCC(0x84), (unsigned long)RCC(0x8c));
    valid &= dmclk_port_sai_release(dmclk_domain_sai2) == 0;
    valid &= RCC(4) == cfgr && RCC(0x88) == lcd && RCC(0x8c) == dck && RCC(0x84) == pll;
    valid &= dmclk_port_sai_release(dmclk_domain_sai2) == -EINVAL;
    return valid ? 0 : -EIO;
}
int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    int result = check_rate(48000);
    if (!result) result = check_rate(44100);
    Dmod_Printf("SAI_CLOCK_HARDWARE %s result=%d\n", result ? "FAIL" : "PASS", result);
    return result;
}
