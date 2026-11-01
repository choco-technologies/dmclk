#define DMOD_ENABLE_REGISTRATION    ON
#include "dmclk_port.h"
#include "../domain.h"
#include <errno.h>

/*
 * ESP32-S3: the CPU runs from the XTAL (40 MHz), the PLL (BBPLL, 480 MHz or 320 MHz: 80, 160 or 240 MHz of the CPU, the APB
 * stays at 80 MHz) or the internal RC oscillator. This port reads the frequency back from the registers and moves the CPU
 * between the PLL frequencies when the PLL is up already (ESP-IDF's second stage bootloader brings it up; the ROM leaves the CPU
 * on the XTAL / 2 and so does the selector of dmod-boot for now).
 *
 * Not done (-ENOTSUP / -ERANGE): bringing the PLL up, going above the frequency that the module was started at (the voltage of
 * the core is set by the bootloader for it), the XTAL/RC sources as targets (the flash and the PSRAM run from the APB clock).
 * No named domains (SDIO, USB, RNG, SAI): the peripherals of the ESP32-S3 have their own clock gates, in their drivers.
 */

#define ESP32S3_SYSTEM_BASE             0x600C0000UL
#define ESP32S3_SYSTEM_CPU_PER_CONF_REG (ESP32S3_SYSTEM_BASE + 0x10UL)
#define ESP32S3_SYSTEM_SYSCLK_CONF_REG  (ESP32S3_SYSTEM_BASE + 0x60UL)

#define CPUPERIOD_SEL_M                 0x3UL           /* 0: 80 MHz, 1: 160 MHz, 2: 240 MHz (of a 480 MHz PLL) */
#define PLL_FREQ_SEL                    (1UL << 2)      /* 1: 480 MHz, 0: 320 MHz */
#define PRE_DIV_CNT_M                   0x3FFUL
#define SOC_CLK_SEL_S                   10U
#define SOC_CLK_SEL_M                   (0x3UL << SOC_CLK_SEL_S)
#define SOC_CLK_SEL_XTAL                0UL
#define SOC_CLK_SEL_PLL                 1UL
#define SOC_CLK_SEL_RC_FAST             2UL

#define XTAL_FREQ_HZ                    40000000ULL     /* the only crystal the chip supports */
#define RC_FAST_FREQ_HZ                 17500000ULL     /* approximate */
#define PLL_480M_HZ                     480000000ULL
#define PLL_320M_HZ                     320000000ULL

/* ROM: the frequency (MHz) that ets_delay_us() and the other functions of the ROM count with */
#define ROM_ETS_UPDATE_CPU_FREQUENCY    0x40001a4cUL

static uint32_t s_max_mhz = 0;      /* the frequency at the start: the voltage of the core allows no more */

static inline volatile uint32_t *reg(uint32_t address)
{
    return (volatile uint32_t *)address;
}

static inline uint32_t ccount(void)
{
    uint32_t value;
    __asm__ __volatile__("rsr.ccount %0" : "=a"(value));
    return value;
}

/** The frequency of the CPU, read back from the hardware. */
static dmclk_frequency_t read_cpu_frequency(void)
{
    uint32_t sysclk = *reg(ESP32S3_SYSTEM_SYSCLK_CONF_REG);
    uint32_t cpu_per = *reg(ESP32S3_SYSTEM_CPU_PER_CONF_REG);
    uint64_t divider = (sysclk & PRE_DIV_CNT_M) + 1U;

    switch ((sysclk & SOC_CLK_SEL_M) >> SOC_CLK_SEL_S)
    {
        case SOC_CLK_SEL_XTAL:
            return XTAL_FREQ_HZ / divider;
        case SOC_CLK_SEL_PLL:
        {
            uint32_t sel = cpu_per & CPUPERIOD_SEL_M;
            if (sel == 0U) return 80000000ULL;
            if (sel == 1U) return 160000000ULL;
            /* 240 MHz exists with the 480 MHz PLL only */
            return (sel == 2U && (cpu_per & PLL_FREQ_SEL)) ? 240000000ULL : 0ULL;
        }
        case SOC_CLK_SEL_RC_FAST:
            return RC_FAST_FREQ_HZ / divider;
        default:
            return 0ULL;
    }
}

/** True when the CPU runs from the PLL, which is the 480 MHz one (80/160/240 MHz) or the 320 MHz one (80/160 MHz). */
static int pll_supports(uint32_t mhz)
{
    uint32_t sysclk = *reg(ESP32S3_SYSTEM_SYSCLK_CONF_REG);
    uint32_t cpu_per = *reg(ESP32S3_SYSTEM_CPU_PER_CONF_REG);
    if (((sysclk & SOC_CLK_SEL_M) >> SOC_CLK_SEL_S) != SOC_CLK_SEL_PLL) return 0;
    return (cpu_per & PLL_FREQ_SEL) ? (mhz == 80U || mhz == 160U || mhz == 240U) : (mhz == 80U || mhz == 160U);
}

static dmclk_frequency_t distance(dmclk_frequency_t a, dmclk_frequency_t b)
{
    return a > b ? a - b : b - a;
}

/** Moves the CPU to the frequency of the PLL that is closest to the target (within the tolerance). */
static int configure_cpu(dmclk_frequency_t target_freq, dmclk_frequency_t tolerance)
{
    dmclk_frequency_t current = read_cpu_frequency();
    if (current != 0U && distance(current, target_freq) <= tolerance)
        return 0;                                   /* it is there already, leave the clocks alone */

    static const uint32_t candidates[] = { 240U, 160U, 80U };
    uint32_t best = 0U;
    for (unsigned i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++)
    {
        dmclk_frequency_t f = (dmclk_frequency_t)candidates[i] * 1000000ULL;
        if (distance(f, target_freq) <= tolerance && (best == 0U || distance(f, target_freq) < distance((dmclk_frequency_t)best * 1000000ULL, target_freq)))
            best = candidates[i];
    }
    if (best == 0U) return -ERANGE;
    if (best > s_max_mhz) return -ERANGE;           /* the core voltage is not raised here */
    if (!pll_supports(best)) return -ENOTSUP;       /* the CPU is not on the PLL: bringing it up is not done */

    uint32_t cpu_per = *reg(ESP32S3_SYSTEM_CPU_PER_CONF_REG);
    *reg(ESP32S3_SYSTEM_CPU_PER_CONF_REG) = (cpu_per & ~CPUPERIOD_SEL_M) | (best == 240U ? 2UL : best == 160U ? 1UL : 0UL);
    ((void (*)(uint32_t))ROM_ETS_UPDATE_CPU_FREQUENCY)(best);
    return 0;
}

/**
 * @brief Initialize the module: remember the frequency that the board was started at
 */
int dmod_init(const Dmod_Config_t *Config)
{
    s_max_mhz = (uint32_t)(read_cpu_frequency() / 1000000ULL);
    Dmod_Printf("DMCLK port module initialized (ESP32-S3, CPU %u MHz)\n", (unsigned)s_max_mhz);
    return 0;
}

int dmod_deinit(void)
{
    int rc = dmclk_domain_begin_configuration();
    if (rc != 0) {
        return rc;
    }
    dmclk_domain_end_configuration();
    Dmod_Printf("DMCLK port module deinitialized (ESP32-S3)\n");
    return 0;
}

/**
 * @brief Set the CPU frequency (80, 160 or 240 MHz of the PLL)
 *
 * The "internal" source is the PLL that the boot has started, from the crystal of the board.
 */
dmod_dmclk_port_api_declaration(1.0, int, _configure_internal, ( dmclk_frequency_t target_freq, dmclk_frequency_t tolerance ))
{
    int rc = dmclk_domain_begin_configuration();
    if (rc != 0) return rc;
    rc = configure_cpu(target_freq, tolerance);
    dmclk_domain_end_configuration();
    return rc;
}

/**
 * @brief The same as the internal one - the PLL is fed by the crystal (40 MHz, the only one that the chip takes)
 */
dmod_dmclk_port_api_declaration(1.0, int, _configure_external, ( dmclk_frequency_t target_freq, dmclk_frequency_t tolerance, dmclk_frequency_t oscillator_freq ))
{
    if (oscillator_freq != 0U && oscillator_freq != XTAL_FREQ_HZ) return -EINVAL;
    int rc = dmclk_domain_begin_configuration();
    if (rc != 0) return rc;
    rc = configure_cpu(target_freq, tolerance);
    dmclk_domain_end_configuration();
    return rc;
}

dmod_dmclk_port_api_declaration(1.0, int, _configure_hibernatation, ( dmclk_frequency_t target_freq, dmclk_frequency_t tolerance, dmclk_frequency_t oscillator_freq ))
{
    (void)target_freq; (void)tolerance; (void)oscillator_freq;
    return -ENOTSUP;
}

/**
 * @brief Busy-wait for the given time on the cycle counter of the core (CCOUNT)
 */
dmod_dmclk_port_api_declaration(1.0, void, _delay_us, ( dmclk_time_us_t time_us ))
{
    uint32_t cycles_per_us = (uint32_t)(read_cpu_frequency() / 1000000ULL);
    if (cycles_per_us == 0U) cycles_per_us = 1U;

    while (time_us > 0U)
    {
        /* up to 10 ms at a time: the counter is 32 bit and wraps */
        uint32_t chunk = time_us > 10000U ? 10000U : (uint32_t)time_us;
        uint32_t cycles = chunk * cycles_per_us;
        uint32_t start = ccount();
        while ((uint32_t)(ccount() - start) < cycles) { }
        time_us -= chunk;
    }
}

dmod_dmclk_port_api_declaration(1.0, dmclk_frequency_t, _get_current_frequency, ( void ))
{
    return read_cpu_frequency();
}

/**
 * @brief Busy-wait for the given number of seconds on CCOUNT and return the cycles that it took
 */
dmod_dmclk_port_api_declaration(1.0, uint64_t, _delay, ( uint32_t seconds ))
{
    if (seconds == 0U) return 0U;

    Dmod_EnterCritical();
    uint64_t target = read_cpu_frequency() * (uint64_t)seconds;
    uint64_t elapsed = 0U;
    uint32_t last = ccount();
    while (elapsed < target)
    {
        uint32_t now = ccount();
        elapsed += (uint32_t)(now - last);
        last = now;
    }
    Dmod_ExitCritical();
    return elapsed;
}

/** There are no named domains: the peripherals of the ESP32-S3 take their clocks from the APB and have their own gates. */
dmod_dmclk_port_api_declaration(1.0, dmclk_frequency_t, _get_domain_frequency, ( dmclk_domain_t domain ))
{
    (void)domain;
    return 0U;
}

int dmclk_domain_acquire_hardware(dmclk_domain_t domain, dmclk_frequency_t target,
                                  dmclk_frequency_t tolerance, dmclk_frequency_t *actual)
{
    (void)domain; (void)target; (void)tolerance; (void)actual;
    return -ENOTSUP;
}

int dmclk_domain_release_hardware(dmclk_domain_t domain)
{
    (void)domain;
    return 0;
}
