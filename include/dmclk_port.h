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
 * @brief Busy-wait delay for a given number of seconds and return consumed CPU cycles.
 *
 * Port implementations should run in a critical section so interrupt latency does not
 * distort the measurement. Implementations may use a hardware cycle counter (preferred)
 * or a port-specific loop fallback.
 *
 * The caller can use the returned CPU cycle count together with a real-world
 * elapsed-time measurement to estimate the actual CPU frequency:
 *
 *   actual_freq_hz = cpu_cycles / actual_elapsed_seconds
 *
 * @param seconds  Number of seconds to busy-wait
 * @return         Total number of CPU cycles consumed by the busy-wait loop
 */
dmod_dmclk_port_api(1.0, uint64_t, _delay, ( uint32_t seconds ) );


#endif // DMCLK_PORT_H