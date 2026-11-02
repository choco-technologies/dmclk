#define DMOD_ENABLE_REGISTRATION    ON
#include "dmclk_port.h"
#include "../domain.h"
#include <errno.h>

/*
 * ESP32-S3: the CPU runs from the XTAL (40 MHz), the PLL (BBPLL, 480 MHz or 320 MHz: 80, 160 or 240 MHz of the CPU, the APB
 * is 80 MHz then) or the internal RC oscillator. The ROM leaves the CPU on the XTAL / 2 (20 MHz) and the board stays there until
 * this port is asked for a frequency: it then brings the BBPLL up (480 MHz, through the analog I2C of the ROM), sets the voltage
 * of the core (from the calibration in the eFuse - what ESP-IDF's second stage bootloader and startup code do) and moves the CPU
 * on the PLL. Going up and down between 80, 160 and 240 MHz follows the same rules (the voltage first when going up, last when
 * going down).
 *
 * Not done (-ERANGE): the XTAL and the RC oscillator as targets (the flash and the PSRAM run from the APB clock).
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

/* ROM: the analog I2C bus (BBPLL, voltage regulators) */
#define ROM_I2C_WRITEREG                0x40005d60UL    /* (block, host, reg, data) */
#define ROM_I2C_WRITEREG_MASK           0x40005d6cUL    /* (block, host, reg, msb, lsb, data) */

#define ANA_CONFIG_REG                  (0x6000E044UL)
#define ANA_CONFIG_S                    8U
#define ANA_CONFIG_M                    0x3FFUL
#define ANA_CONFIG_I2C_BBPLL_M          (1UL << 17)
#define I2C_MST_ANA_CONF0_REG           (0x6000E040UL)
#define I2C_MST_BBPLL_STOP_FORCE_HIGH   (1UL << 2)
#define I2C_MST_BBPLL_STOP_FORCE_LOW    (1UL << 3)
#define I2C_MST_BBPLL_CAL_DONE          (1UL << 24)

#define RTC_CNTL_OPTIONS0_REG           (0x60008000UL)
#define RTC_CNTL_BB_I2C_FORCE_PD        (1UL << 6)
#define RTC_CNTL_BBPLL_I2C_FORCE_PD     (1UL << 8)
#define RTC_CNTL_BBPLL_FORCE_PD         (1UL << 10)
#define RTC_CNTL_DATE_REG               (0x600080FCUL)  /* [5:0]: the slaves of the LDO that are powered down */
#define RTC_CNTL_SLAVE_PD_M             0x3FUL
#define DEFAULT_LDO_SLAVE               0x7U

#define I2C_BBPLL                       0x66U
#define I2C_BBPLL_HOSTID                1U
#define I2C_DIG_REG                     0x6DU
#define I2C_DIG_REG_HOSTID              1U

#define EFUSE_BASE                      0x60007000UL
#define EFUSE_RD_MAC_SPI_SYS_3_REG      (EFUSE_BASE + 0x50UL)
#define EFUSE_RD_MAC_SPI_SYS_4_REG      (EFUSE_BASE + 0x54UL)
#define EFUSE_RD_MAC_SPI_SYS_5_REG      (EFUSE_BASE + 0x58UL)
#define EFUSE_RD_SYS_PART1_DATA4_REG    (EFUSE_BASE + 0x6cUL)

/* the calibration of the voltage regulators in the eFuse (the constants are the ones of ESP-IDF's soc/rtc.h) */
#define K_RTC_MID_MUL10000              198
#define K_DIG_MID_MUL10000              211
#define V_RTC_MID_MUL10000              10181
#define V_DIG_MID_MUL10000              10841

/* the clock of the SPI0 (cache) and SPI1 (the flash driver) controllers: both the flash and the PSRAM run from the APB clock divided */
#define SPI_MEM_CLOCK_REG_SPI1          (0x60002014UL)
#define SPI_MEM_CLOCK_REG_SPI0          (0x60003014UL)
#define SPI_MEM_CLOCK_DIV4              0x00030103UL    /* N = 3, H = 1, L = 3: APB / 4 */
#define SPI_MEM_CLOCK_DIV_M             0x80FFFFFFUL

static int s_dbias_ready = 0;
static uint32_t s_dig_dbias_240m = 28U;         /* the values of ESP-IDF until the eFuse says better */
static uint32_t s_rtc_dbias_240m = 28U;
static uint32_t s_dig_dbias_non_240m = 27U;
static uint32_t s_rtc_dbias_non_240m = 27U;

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

static void i2c_write(uint8_t block, uint8_t host, uint8_t addr, uint8_t data)
{
    ((void (*)(uint8_t, uint8_t, uint8_t, uint8_t))ROM_I2C_WRITEREG)(block, host, addr, data);
}

static void i2c_write_mask(uint8_t block, uint8_t host, uint8_t addr, uint8_t msb, uint8_t lsb, uint8_t data)
{
    ((void (*)(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t))ROM_I2C_WRITEREG_MASK)(block, host, addr, msb, lsb, data);
}

/** Busy-wait on CCOUNT; the frequency is the one that the CPU runs at right now. */
static void spin_us(uint32_t us, uint32_t mhz)
{
    uint32_t cycles = us * mhz;
    uint32_t start = ccount();
    while ((uint32_t)(ccount() - start) < cycles) { }
}

static int32_t efuse_signed(uint32_t value, uint32_t sign_bit)
{
    return (value & sign_bit) ? -(int32_t)(value & (sign_bit - 1U)) : (int32_t)(uint8_t)value;
}

static uint32_t rtc_dbias_for(uint32_t dig_dbias)
{
    uint32_t sys4 = *reg(EFUSE_RD_MAC_SPI_SYS_4_REG);
    uint32_t sys5 = *reg(EFUSE_RD_MAC_SPI_SYS_5_REG);
    int32_t k_rtc = efuse_signed((sys4 >> 13) & 0x7FU, 1U << 6);
    int32_t k_dig = efuse_signed((sys4 >> 20) & 0x7FU, 1U << 6);
    int32_t v_rtc20 = efuse_signed((((sys5 & 0x7U) << 5) | ((sys4 >> 27) & 0x1FU)), 1U << 7);
    int32_t v_dig20 = efuse_signed((sys5 >> 3) & 0xFFU, 1U << 7);

    uint32_t v_rtc20_real = (uint32_t)(V_RTC_MID_MUL10000 + v_rtc20 * 10000 / 500);
    uint32_t v_dig20_real = (uint32_t)(V_DIG_MID_MUL10000 + v_dig20 * 10000 / 500);
    int32_t k_rtc_real = K_RTC_MID_MUL10000 + k_rtc;
    int32_t k_dig_real = K_DIG_MID_MUL10000 + k_dig;
    uint32_t v_dig_target = (uint32_t)((int32_t)v_dig20_real + k_dig_real * ((int32_t)dig_dbias - 20));
    uint32_t rtc_dbias;
    for (rtc_dbias = 15U; rtc_dbias < 31U; rtc_dbias++)
    {
        uint32_t v_rtc = (uint32_t)((int32_t)v_rtc20_real + k_rtc_real * ((int32_t)rtc_dbias - 20));
        if (v_rtc >= v_dig_target - 250U) break;
    }
    return rtc_dbias;
}

/** The voltage of the core for the PLL frequencies, from the calibration burnt in the eFuse (what ESP-IDF's rtc_init does). */
static void load_dbias(void)
{
    if (s_dbias_ready) return;
    s_dbias_ready = 1;

    uint32_t sys3 = *reg(EFUSE_RD_MAC_SPI_SYS_3_REG);
    uint32_t part1 = *reg(EFUSE_RD_SYS_PART1_DATA4_REG);
    uint32_t minor = (sys3 >> 24) & 0x7U;
    uint32_t major = part1 & 0x3U;
    /* the PVT scheme exists from the block version 1.2 */
    if (!((major <= 1U && minor == 1U) || major > 1U || (major == 1U && minor >= 2U))) return;

    uint32_t sys4 = *reg(EFUSE_RD_MAC_SPI_SYS_4_REG);
    uint32_t sys5 = *reg(EFUSE_RD_MAC_SPI_SYS_5_REG);
    int32_t k_dig = efuse_signed((sys4 >> 20) & 0x7FU, 1U << 6);
    int32_t v_dig20 = efuse_signed((sys5 >> 3) & 0xFFU, 1U << 7);
    uint32_t dig_dbias = (sys5 >> 11) & 0x1FU;                  /* the value that makes 240 MHz run, burnt by Espressif */
    if (dig_dbias == 0U) return;                                /* not burnt: the defaults stay */

    uint32_t v_dig20_real = (uint32_t)(V_DIG_MID_MUL10000 + v_dig20 * 10000 / 500);
    int32_t k_dig_real = K_DIG_MID_MUL10000 + k_dig;
    uint32_t dig_1v3;
    for (dig_1v3 = 15U; dig_1v3 < 31U; dig_1v3++)
    {
        if ((uint32_t)((int32_t)v_dig20_real + k_dig_real * ((int32_t)dig_1v3 - 20)) >= 13000U) break;
    }
    s_dig_dbias_240m = dig_dbias + 3U < dig_1v3 ? dig_dbias + 3U : dig_1v3;
    s_dig_dbias_non_240m = dig_dbias + 2U < dig_1v3 ? dig_dbias + 2U : dig_1v3;
    s_rtc_dbias_240m = rtc_dbias_for(s_dig_dbias_240m);
    s_rtc_dbias_non_240m = rtc_dbias_for(s_dig_dbias_non_240m);
}

static void set_dbias(int is_240m)
{
    i2c_write_mask(I2C_DIG_REG, I2C_DIG_REG_HOSTID, 4U, 4U, 0U, (uint8_t)(is_240m ? s_rtc_dbias_240m : s_rtc_dbias_non_240m));
    i2c_write_mask(I2C_DIG_REG, I2C_DIG_REG_HOSTID, 6U, 4U, 0U, (uint8_t)(is_240m ? s_dig_dbias_240m : s_dig_dbias_non_240m));
}

static void set_ldo_slaves(uint32_t value)
{
    *reg(RTC_CNTL_DATE_REG) = (*reg(RTC_CNTL_DATE_REG) & ~RTC_CNTL_SLAVE_PD_M) | (value & RTC_CNTL_SLAVE_PD_M);
}

/** Starts the BBPLL at 480 MHz from the 40 MHz crystal and waits for its calibration. The CPU stays where it is. */
static int pll_start(void)
{
    /* the analog I2C bus: reset and let the BBPLL registers in */
    *reg(ANA_CONFIG_REG) |= (ANA_CONFIG_M << ANA_CONFIG_S);
    *reg(ANA_CONFIG_REG) &= ~ANA_CONFIG_I2C_BBPLL_M;

    *reg(RTC_CNTL_OPTIONS0_REG) &= ~(RTC_CNTL_BB_I2C_FORCE_PD | RTC_CNTL_BBPLL_FORCE_PD | RTC_CNTL_BBPLL_I2C_FORCE_PD);
    *reg(ESP32S3_SYSTEM_CPU_PER_CONF_REG) |= PLL_FREQ_SEL;      /* 480 MHz */

    *reg(I2C_MST_ANA_CONF0_REG) &= ~I2C_MST_BBPLL_STOP_FORCE_HIGH;
    *reg(I2C_MST_ANA_CONF0_REG) |= I2C_MST_BBPLL_STOP_FORCE_LOW;

    /* 480 MHz from 40 MHz: ref div 0, div 8, dr1/dr3 0, dchgp 5, dcur 3 */
    i2c_write(I2C_BBPLL, I2C_BBPLL_HOSTID, 4U, 0x6BU);          /* MODE_HF */
    i2c_write(I2C_BBPLL, I2C_BBPLL_HOSTID, 2U, (5U << 4) | 0U); /* OC_REF_DIV | OC_DCHGP */
    i2c_write(I2C_BBPLL, I2C_BBPLL_HOSTID, 3U, 8U);             /* OC_DIV_7_0 */
    i2c_write_mask(I2C_BBPLL, I2C_BBPLL_HOSTID, 5U, 2U, 0U, 0U);        /* OC_DR1 */
    i2c_write_mask(I2C_BBPLL, I2C_BBPLL_HOSTID, 5U, 6U, 4U, 0U);        /* OC_DR3 */
    i2c_write(I2C_BBPLL, I2C_BBPLL_HOSTID, 6U, (1U << 6) | (3U << 4) | 3U);     /* OC_DCUR with the DLREF/DHREF selects */
    i2c_write_mask(I2C_BBPLL, I2C_BBPLL_HOSTID, 9U, 1U, 0U, 3U);        /* OC_VCO_DBIAS */

    uint32_t guard = 1000000U;
    while (!(*reg(I2C_MST_ANA_CONF0_REG) & I2C_MST_BBPLL_CAL_DONE))
    {
        if (--guard == 0U) return -ETIMEDOUT;
    }
    spin_us(10U, 20U);                                          /* the CPU is on the XTAL / 2 (20 MHz) or faster: 10 us at the least */
    *reg(I2C_MST_ANA_CONF0_REG) &= ~I2C_MST_BBPLL_STOP_FORCE_LOW;
    *reg(I2C_MST_ANA_CONF0_REG) |= I2C_MST_BBPLL_STOP_FORCE_HIGH;
    return 0;
}

/** Moves the CPU to 80, 160 or 240 MHz of the PLL, starting the PLL when the CPU is not on it. */
static int cpu_to_pll(uint32_t mhz)
{
    load_dbias();
    uint32_t cur_mhz = (uint32_t)(read_cpu_frequency() / 1000000ULL);
    int on_pll = pll_supports(80U);
    int rc;

    if (on_pll && !pll_supports(mhz)) return -ENOTSUP;         /* 240 MHz needs the 480 MHz PLL */

    Dmod_EnterCritical();
    if (!on_pll)
    {
        /* the voltage first: the PLL is the way to the higher frequencies */
        set_dbias(mhz == 240U);
        rc = pll_start();
        if (rc != 0)
        {
            Dmod_ExitCritical();
            return rc;
        }
        cur_mhz = cur_mhz == 0U ? 20U : cur_mhz;
    }
    else if (mhz > cur_mhz && mhz == 240U)
    {
        set_dbias(1);
        spin_us(40U, cur_mhz);
    }
    if (mhz > cur_mhz || !on_pll)
        set_ldo_slaves(DEFAULT_LDO_SLAVE >> (mhz / 80U));

    if (!on_pll)
    {
        /* the APB goes from 20 MHz to 80 MHz with the PLL: the flash gets a divider that keeps its clock at 20 MHz or lower */
        *reg(SPI_MEM_CLOCK_REG_SPI1) = (*reg(SPI_MEM_CLOCK_REG_SPI1) & ~SPI_MEM_CLOCK_DIV_M) | SPI_MEM_CLOCK_DIV4;
        *reg(SPI_MEM_CLOCK_REG_SPI0) = (*reg(SPI_MEM_CLOCK_REG_SPI0) & ~SPI_MEM_CLOCK_DIV_M) | SPI_MEM_CLOCK_DIV4;
    }

    uint32_t cpu_per = *reg(ESP32S3_SYSTEM_CPU_PER_CONF_REG);
    *reg(ESP32S3_SYSTEM_CPU_PER_CONF_REG) = (cpu_per & ~CPUPERIOD_SEL_M) | (mhz == 240U ? 2UL : mhz == 160U ? 1UL : 0UL);
    uint32_t sysclk = *reg(ESP32S3_SYSTEM_SYSCLK_CONF_REG);
    sysclk &= ~(PRE_DIV_CNT_M | SOC_CLK_SEL_M);                 /* divider 1 */
    *reg(ESP32S3_SYSTEM_SYSCLK_CONF_REG) = sysclk | (SOC_CLK_SEL_PLL << SOC_CLK_SEL_S);
    ((void (*)(uint32_t))ROM_ETS_UPDATE_CPU_FREQUENCY)(mhz);

    if (on_pll && mhz < cur_mhz)
    {
        if (cur_mhz == 240U)
        {
            set_dbias(0);
            spin_us(40U, mhz);
        }
        set_ldo_slaves(DEFAULT_LDO_SLAVE >> (mhz / 80U));
    }
    Dmod_ExitCritical();
    return 0;
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
    return cpu_to_pll(best);
}

/**
 * @brief Initialize the module
 */
int dmod_init(const Dmod_Config_t *Config)
{
    Dmod_Printf("DMCLK port module initialized (ESP32-S3, CPU %u MHz)\n", (unsigned)(read_cpu_frequency() / 1000000ULL));
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
