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
 *        Used by dmclk_test to convert loop iteration count to actual CPU cycles,
 *        allowing calculation of the real clock frequency from a measured elapsed time.
 */
#define DMCLK_PORT_DELAY_CYCLES_PER_ITERATION  4U

/**
 * @brief Busy-wait delay for a given number of seconds using a counted NOP loop.
 *
 * The number of loop iterations is derived from the currently configured clock
 * frequency.  The loop runs inside a critical section (interrupts disabled) so
 * that context switches cannot distort the iteration count.
 *
 * Together with DMCLK_PORT_DELAY_CYCLES_PER_ITERATION the caller can estimate
 * the actual CPU frequency from the number of iterations and a real-world
 * elapsed-time measurement:
 *
 *   actual_freq_hz = iterations * DMCLK_PORT_DELAY_CYCLES_PER_ITERATION
 *                    / actual_elapsed_seconds
 *
 * @param seconds  Number of seconds to busy-wait
 * @return         Number of NOP loop iterations that were executed
 */
dmod_dmclk_port_api(1.0, uint64_t, _delay, ( uint32_t seconds ) );


#endif // DMCLK_PORT_H