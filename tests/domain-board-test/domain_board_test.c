#include "dmclk_port.h"
#include "port/stm32f7_regs.h"
#include <errno.h>

#define REG(offset) (*(volatile uint32_t *)(STM32F7_RCC_BASE + (offset)))

static int failures;
#define CHECK(condition) do { if (!(condition)) { \
    Dmod_Printf("FAIL line %d: %s\n", __LINE__, #condition); ++failures; \
} } while (0)

/* Run as `domain_board_test` from dmell on STM32F746G-DISCO. No direct writes
 * to hardware: the public port API owns all clock changes and their cleanup. */
int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    const uint32_t offsets[] = {0, 4, 8, 0x84, 0x88, 0x8c, 0x90};
    uint32_t before[sizeof(offsets) / sizeof(offsets[0])];
    for (unsigned i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) { before[i] = REG(offsets[i]); }
    dmclk_frequency_t sysclk = dmclk_port_get_current_frequency();
    dmclk_frequency_t clk48 = dmclk_port_get_domain_frequency(dmclk_domain_sdio);
    Dmod_Printf("DOMAIN TEST: SYSCLK=%llu CLK48=%llu PLLSAI=0x%08x\n",
                (unsigned long long)sysclk, (unsigned long long)clk48, (unsigned)REG(0x88));
    const dmclk_frequency_t targets[] = {12288000, 11289600};
    const dmclk_frequency_t tolerances[] = {3000, 1000};
    for (unsigned i = 0; i < 2; ++i) {
        dmclk_frequency_t actual = 0, shared = 0, unchanged = 123;
        int rc = dmclk_port_acquire_domain(dmclk_domain_sai2, targets[i], tolerances[i], &actual);
        Dmod_Printf("SAI2 acquire target=%llu actual=%llu rc=%d\n",
                    (unsigned long long)targets[i], (unsigned long long)actual, rc);
        CHECK(rc == 0);
        if (rc != 0) { continue; }
        CHECK((actual > targets[i] ? actual - targets[i] : targets[i] - actual) <= tolerances[i]);
        CHECK(dmclk_port_get_domain_frequency(dmclk_domain_sai2) == actual);
        CHECK(dmclk_port_configure_internal(216000000, 1000) == -EBUSY);
        CHECK(dmclk_port_configure_external(216000000, 1000, 25000000) == -EBUSY);
        CHECK(dmclk_port_configure_hibernatation(32000, 1000, 32000) == -EBUSY);
        CHECK(dmclk_port_acquire_domain(dmclk_domain_sai2, targets[1-i], tolerances[1-i], &unchanged) == -EBUSY);
        CHECK(unchanged == 123);
        rc = dmclk_port_acquire_domain(dmclk_domain_sai2, targets[i], tolerances[i], &shared);
        CHECK(rc == 0);
        if (rc == 0) {
            CHECK(shared == actual);
            CHECK(dmclk_port_release_domain(dmclk_domain_sai2) == 0);
            CHECK(dmclk_port_get_domain_frequency(dmclk_domain_sai2) == actual);
        }
        /* Another SAI controller may share the same physical PLL. */
        rc = dmclk_port_acquire_domain(dmclk_domain_sai1, targets[i], tolerances[i], &shared);
        CHECK(rc == 0);
        CHECK(dmclk_port_release_domain(dmclk_domain_sai2) == 0);
        if (rc == 0) {
            CHECK(dmclk_port_get_domain_frequency(dmclk_domain_sai1) == actual);
            CHECK(dmclk_port_release_domain(dmclk_domain_sai1) == 0);
        }
        CHECK(dmclk_port_release_domain(dmclk_domain_sai2) == -EINVAL);
        CHECK(dmclk_port_get_current_frequency() == sysclk);
        CHECK(dmclk_port_get_domain_frequency(dmclk_domain_sdio) == clk48);
        CHECK(dmclk_port_get_domain_frequency(dmclk_domain_usb) == clk48);
        CHECK(dmclk_port_get_domain_frequency(dmclk_domain_rng) == clk48);
        for (unsigned j = 0; j < sizeof(offsets) / sizeof(offsets[0]); ++j) {
            CHECK(REG(offsets[j]) == before[j]);
        }
    }
    Dmod_Printf("DOMAIN BOARD TEST: %s (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
