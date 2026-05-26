#define DMOD_ENABLE_REGISTRATION
#include "dmod.h"
#include "dmclk_port.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/**
 * @brief dmclk_test – CPU clock frequency measurement application.
 *
 * Usage:
 *   dmclk_test <seconds>
 *
 * The application calls dmclk_port_delay() which busy-waits for the requested
 * number of seconds (with interrupts disabled) using a counted ASM loop.  The
 * user then measures the actual elapsed time with an external clock and enters
 * it when prompted.  The application uses the returned CPU cycle count and the
 * measured elapsed time to calculate and display the real CPU clock frequency.
 *
 * Calculation:
 *   actual_freq_hz = cpu_cycles / actual_elapsed_seconds
 */

static int dmclk_test_main(int argc, char* argv[])
{
    Dmod_Printf("\n=== DMCLK Clock Frequency Measurement ===\n\n");

    if (argc < 2)
    {
        Dmod_Printf("Usage: dmclk_test <seconds>\n");
        Dmod_Printf("Example: dmclk_test 3600\n\n");
        Dmod_Printf("The application busy-waits for the given number of seconds\n");
        Dmod_Printf("and then asks for the actual elapsed time to calculate the\n");
        Dmod_Printf("real CPU clock frequency.\n");
        return -1;
    }

    uint32_t seconds = (uint32_t)strtoul(argv[1], NULL, 10);
    if (seconds == 0)
    {
        Dmod_Printf("Error: invalid or zero delay: '%s'\n", argv[1]);
        return -1;
    }

    dmclk_frequency_t nominal_freq = dmclk_port_get_current_frequency();
    Dmod_Printf("Nominal clock frequency : %llu Hz (%llu MHz)\n",
                (unsigned long long)nominal_freq,
                (unsigned long long)(nominal_freq / 1000000ULL));
    Dmod_Printf("Requested delay         : %u second(s)\n\n", seconds);
    Dmod_Printf("Starting busy-wait loop (critical section)...\n");

    uint64_t cpu_cycles = dmclk_port_delay(seconds);

    Dmod_Printf("Delay complete.\n");
    Dmod_Printf("CPU cycles counted     : %llu\n\n", (unsigned long long)cpu_cycles);

    Dmod_Printf("Using a stopwatch or external reference clock, determine how\n");
    Dmod_Printf("many seconds actually elapsed during the busy-wait above.\n\n");
    Dmod_Printf("Enter actual elapsed time in seconds (integer): ");

    uint32_t actual_seconds = 0;
    if (scanf("%u", &actual_seconds) != 1 || actual_seconds == 0)
    {
        Dmod_Printf("\nError: could not read a valid elapsed time.\n");
        return -1;
    }

    /* actual_freq = cpu_cycles / actual_elapsed_s */
    uint64_t actual_freq = cpu_cycles / (uint64_t)actual_seconds;

    int64_t diff = (int64_t)actual_freq - (int64_t)nominal_freq;

    Dmod_Printf("\n=== Results ===\n");
    Dmod_Printf("Nominal frequency : %llu Hz (%llu MHz)\n",
                (unsigned long long)nominal_freq,
                (unsigned long long)(nominal_freq / 1000000ULL));
    Dmod_Printf("Actual frequency  : %llu Hz (%llu MHz)\n",
                (unsigned long long)actual_freq,
                (unsigned long long)(actual_freq / 1000000ULL));
    if (diff >= 0)
    {
        Dmod_Printf("Difference        : +%lld Hz (+%lld kHz)\n",
                    (long long)diff, (long long)(diff / 1000));
    }
    else
    {
        Dmod_Printf("Difference        : %lld Hz (%lld kHz)\n",
                    (long long)diff, (long long)(diff / 1000));
    }
    Dmod_Printf("\n");

    return 0;
}

DMOD_MAIN(dmclk_test, dmclk_test_main)
