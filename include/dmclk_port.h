#ifndef DMCLK_PORT_H
#define DMCLK_PORT_H

#include "dmod.h"
#include "dmclk_port_defs.h"
#include "dmclk_types.h"

dmod_dmclk_port_api(1.0, int, _configure_internal, ( dmclk_frequency_t target_freq, dmclk_frequency_t tolerance) );
dmod_dmclk_port_api(1.0, int, _configure_external, ( dmclk_frequency_t target_freq, dmclk_frequency_t tolerance, dmclk_frequency_t oscillator_freq) );
dmod_dmclk_port_api(1.0, int, _configure_hibernatation, ( dmclk_frequency_t target_freq, dmclk_frequency_t tolerance, dmclk_frequency_t oscillator_freq) );
dmod_dmclk_port_api(1.0, void, _delay_us, ( dmclk_time_us_t time_us) );
dmod_dmclk_port_api(1.0, dmclk_frequency_t, _get_current_frequency, ( void ) );

/**
 * @brief Get the actual, currently-programmed frequency of a named
 * peripheral clock domain.
 *
 * Read back from hardware registers rather than cached from the last
 * configuration request, the same as _get_current_frequency().
 *
 * @param domain Which named domain to query
 * @return dmclk_frequency_t Frequency in Hz, or 0 if this port has no such
 *         domain or it isn't currently active
 */
dmod_dmclk_port_api(1.0, dmclk_frequency_t, _get_domain_frequency, ( dmclk_domain_t domain ) );

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


/** Reserve a SAI kernel clock. Thread context only; -EBUSY on conflict.
 * target/tolerance are Hz. actual is only written on success. Compatible
 * acquisitions share PLLI2S; release once per successful acquisition.
 * Main clock/hibernation configuration and port deinit are blocked while
 * reserved or failed-acquire recovery is pending. Up to 255 references per
 * domain; -EOVERFLOW beyond that. See docs/sai-clock.md for timeout recovery.
 * Ports without this clock path return -ENOTSUP. */
dmod_dmclk_port_api(1.0, int, _sai_acquire, ( dmclk_domain_t domain, dmclk_frequency_t target, dmclk_frequency_t tolerance, dmclk_frequency_t *actual ) );
dmod_dmclk_port_api(1.0, int, _sai_release, ( dmclk_domain_t domain ) );

#endif // DMCLK_PORT_H