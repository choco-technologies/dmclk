#include "dmclk_port.h"
#include "clock_fixture.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define TARGET_48K 12288000U
#define TOLERANCE_48K 6144U

static int acquire(dmclk_frequency_t *actual)
{
    return dmclk_port_sai_acquire(dmclk_domain_sai2, TARGET_48K, TOLERANCE_48K, actual);
}

static void rates(void)
{
    const uint32_t rates[] = {8000, 11025, 16000, 22050, 32000, 44100, 48000, 96000};
    for (unsigned i = 0; i < sizeof(rates) / sizeof(*rates); i++)
    {
        uint32_t target = rates[i] * 256, tolerance = target / 2000;
        dmclk_frequency_t actual = 0;
        assert(dmclk_port_sai_acquire(dmclk_domain_sai2, target, tolerance, &actual) == 0);
        assert(actual >= target - tolerance && actual <= target + tolerance);
        assert(dmclk_port_get_domain_frequency(dmclk_domain_sai2) == actual);
        assert(dmclk_port_sai_release(dmclk_domain_sai2) == 0);
        assert(clock_fixture_unchanged());
    }
}

static void ownership(void)
{
    dmclk_frequency_t actual = 0, shared = 0;
    assert(acquire(&actual) == 0);
    /* A rounded frequency is not an exact rational match. */
    assert(dmclk_port_sai_acquire(dmclk_domain_sai2, actual, 0, &shared) == -EBUSY);
    assert(dmclk_port_sai_acquire(dmclk_domain_sai1, TARGET_48K, TOLERANCE_48K, &shared) == 0);
    assert(shared == actual);
    assert(dmclk_port_sai_acquire(dmclk_domain_sai2, 11289600, 5644, &shared) == -EBUSY);
    assert(acquire(&shared) == 0);
    assert(dmclk_port_sai_release(dmclk_domain_sai2) == 0);
    assert(dmclk_port_get_domain_frequency(dmclk_domain_sai2) == actual);
    clock_fixture_gate_sai(true);
    assert(dmclk_port_sai_release(dmclk_domain_sai2) == -EBUSY);
    clock_fixture_gate_sai(false);
    assert(dmclk_port_sai_release(dmclk_domain_sai2) == 0);
    assert(dmclk_port_get_domain_frequency(dmclk_domain_sai1) == actual);
    assert(dmclk_port_sai_release(dmclk_domain_sai1) == 0);
    assert(clock_fixture_unchanged());
    assert(dmclk_port_sai_release(dmclk_domain_sai1) == -EINVAL);
    clock_fixture_external_owner(true, false);
    assert(acquire(&actual) == -EBUSY);
    clock_fixture_external_owner(false, true);
    assert(acquire(&actual) == -EBUSY);
}

static void invalid(void)
{
    dmclk_frequency_t actual = 999;
    assert(dmclk_port_sai_acquire(dmclk_domain_usb, TARGET_48K, 6144, &actual) == -EINVAL);
    assert(dmclk_port_sai_acquire(dmclk_domain_sai2, 0, 6144, &actual) == -EINVAL);
    assert(dmclk_port_sai_acquire(dmclk_domain_sai2, TARGET_48K, 6144, NULL) == -EINVAL);
    assert(dmclk_port_sai_acquire(dmclk_domain_sai2, TARGET_48K, 0, &actual) == -ERANGE);
    assert(dmclk_port_sai_acquire(dmclk_domain_sai2, UINT64_MAX, 6144, &actual) == -EINVAL);
    assert(dmclk_port_sai_acquire(dmclk_domain_sai2, TARGET_48K, UINT64_MAX, &actual) == -EINVAL);
    assert(actual == 999 && clock_fixture_unchanged());
}

static void source(void)
{
    dmclk_frequency_t actual = 999;
    clock_fixture_source(false, false);
    clock_fixture_snapshot();
    assert(acquire(&actual) == -ENODATA);
    assert(actual == 999 && clock_fixture_unchanged());
    clock_fixture_source(true, true); /* Bootloader-owned HSE, frequency unknown. */
    clock_fixture_snapshot();
    assert(acquire(&actual) == -ENODATA);
    assert(actual == 999 && clock_fixture_unchanged());
}

static void overflow(void)
{
    dmclk_frequency_t actual = 0;
    for (unsigned i = 0; i < UINT8_MAX; i++)
        assert(acquire(&actual) == 0);
    actual = 999;
    assert(acquire(&actual) == -EOVERFLOW);
    assert(actual == 999);
    for (unsigned i = 0; i < UINT8_MAX; i++)
        assert(dmclk_port_sai_release(dmclk_domain_sai2) == 0);
    assert(clock_fixture_unchanged());
}

static void lock_timeout(void)
{
    dmclk_frequency_t actual = 999;
    clock_fixture_fault(true, false);
    assert(acquire(&actual) == -ETIMEDOUT);
    assert(clock_fixture_elapsed_ms() >= 100);
    assert(actual == 999 && clock_fixture_unchanged());
    clock_fixture_fault(false, false);
    assert(acquire(&actual) == 0);
    assert(dmclk_port_sai_release(dmclk_domain_sai2) == 0);
    assert(clock_fixture_unchanged());
}

static void stop_timeout(void)
{
    dmclk_frequency_t actual = 0;
    assert(acquire(&actual) == 0);
    clock_fixture_fault(false, true);
    assert(dmclk_port_sai_release(dmclk_domain_sai2) == -ETIMEDOUT);
    assert(clock_fixture_deinit() == -EBUSY);
    clock_fixture_fault(false, false);
    assert(dmclk_port_sai_release(dmclk_domain_sai2) == 0);
    assert(clock_fixture_unchanged());
}

static void rollback(void)
{
    dmclk_frequency_t actual = 999;
    clock_fixture_fault(true, true);
    assert(acquire(&actual) == -ETIMEDOUT);
    assert(actual == 999 && !clock_fixture_unchanged());
    assert(clock_fixture_deinit() == -EBUSY);
    assert(dmclk_port_configure_internal(16000000, 0) == -EBUSY);
    assert(acquire(&actual) == -ETIMEDOUT);
    clock_fixture_fault(false, false);
    assert(acquire(&actual) == 0);
    assert(dmclk_port_sai_release(dmclk_domain_sai2) == 0);
    assert(clock_fixture_unchanged()); /* The original snapshot survived retries. */
}

static void concurrency(void)
{
    dmclk_frequency_t actual;
    clock_fixture_concurrent_call();
    assert(acquire(&actual) == 0);
    assert(clock_fixture_concurrent_result() == -EBUSY);
    assert(dmclk_port_sai_release(dmclk_domain_sai2) == 0);
    assert(clock_fixture_unchanged());
}

static void guards(void)
{
    dmclk_frequency_t actual;
    assert(acquire(&actual) == 0);
    clock_fixture_snapshot();
    assert(dmclk_port_configure_internal(216000000, 1000) == -EBUSY);
    assert(dmclk_port_configure_external(216000000, 1000, 25000000) == -EBUSY);
    assert(dmclk_port_configure_hibernatation(32000, 1000, 0) == -EBUSY);
    assert(clock_fixture_deinit() == -EBUSY);
    assert(clock_fixture_unchanged());
    assert(dmclk_port_sai_release(dmclk_domain_sai2) == 0);
    assert(clock_fixture_deinit() == 0);
}

static void unsupported(void)
{
    clock_fixture_init(false);
    dmclk_frequency_t actual = 999;
    assert(acquire(&actual) == -ENOTSUP);
    assert(dmclk_port_sai_release(dmclk_domain_sai2) == -ENOTSUP);
    assert(dmclk_port_get_domain_frequency(dmclk_domain_sai2) == 0);
    assert(actual == 999 && clock_fixture_unchanged());
}

int main(int argc, char **argv)
{
    const struct { const char *name; void (*run)(void); } cases[] = {
        {"rates", rates}, {"ownership", ownership}, {"invalid", invalid},
        {"source", source}, {"overflow", overflow}, {"lock_timeout", lock_timeout},
        {"stop_timeout", stop_timeout}, {"rollback", rollback}, {"concurrency", concurrency},
        {"guards", guards}, {"unsupported", unsupported}
    };
    assert(argc == 2);
    for (unsigned i = 0; i < sizeof(cases) / sizeof(*cases); i++)
    {
        if (strcmp(argv[1], cases[i].name) != 0)
            continue;
        clock_fixture_init(true);
        cases[i].run();
        puts("PASS");
        return 0;
    }
    return 1;
}
