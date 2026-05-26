#ifndef DMCLK_PORT_H
#define DMCLK_PORT_H

#include "dmod.h"
#include "dmclk_port_defs.h"

/**
 * @brief Clock frequency type in Hz
 */
typedef uint64_t dmclk_frequency_t;

/**
 * @brief Time type in microseconds
 */
typedef uint64_t dmclk_time_us_t;

dmod_dmclk_port_api(1.0, int, _configure_internal, ( dmclk_frequency_t target_freq, dmclk_frequency_t tolerance) );
dmod_dmclk_port_api(1.0, int, _configure_external, ( dmclk_frequency_t target_freq, dmclk_frequency_t tolerance, dmclk_frequency_t oscillator_freq) );
dmod_dmclk_port_api(1.0, int, _configure_hibernatation, ( dmclk_frequency_t target_freq, dmclk_frequency_t tolerance, dmclk_frequency_t oscillator_freq) );
dmod_dmclk_port_api(1.0, void, _delay_us, ( dmclk_time_us_t time_us) );
dmod_dmclk_port_api(1.0, dmclk_frequency_t, _get_current_frequency, ( void ) );

/**
 * @brief Number of CPU cycles consumed per iteration of the dmclk_port_delay busy-wait loop.
 *        The loop body is a single ARM `SUBS Rn, Rn, #1` + `BNE` pair which costs exactly
 *        2 cycles on both Cortex-M4 and Cortex-M7 (SUBS sets flags, BNE reads them —
 *        the data dependency prevents dual-issue on M7 as well).
 *        Used by dmclk_test to reconstruct actual CPU frequency from a measured elapsed time.
 */
#define DMCLK_PORT_DELAY_CYCLES_PER_ITERATION  2U

/**
 * @brief Busy-wait delay for a given number of seconds using a precisely cycle-counted
 *        ASM loop running inside a critical section.
 *
 * The number of loop iterations per second is derived from the currently configured
 * clock frequency divided by DMCLK_PORT_DELAY_CYCLES_PER_ITERATION.  The inner loop
 * is written in ARM inline assembly so the cycle count per iteration is deterministic
 * and independent of compiler optimisation settings:
 *
 *   1:  SUBS Rn, Rn, #1    ; 1 cycle
 *       BNE  1b             ; 1 cycle (taken, flag dependency prevents dual-issue on M7)
 *
 * The loop runs inside a critical section (Dmod_EnterCritical / Dmod_ExitCritical) so
 * that interrupt latency cannot distort the iteration count.
 *
 * The caller can use the returned iteration count with a real-world elapsed-time
 * measurement to estimate the actual CPU frequency:
 *
 *   actual_freq_hz = iterations * DMCLK_PORT_DELAY_CYCLES_PER_ITERATION
 *                    / actual_elapsed_seconds
 *
 * @param seconds  Number of seconds to busy-wait
 * @return         Total number of ASM loop iterations executed
 */
dmod_dmclk_port_api(1.0, uint64_t, _delay, ( uint32_t seconds ) );


#endif // DMCLK_PORT_H