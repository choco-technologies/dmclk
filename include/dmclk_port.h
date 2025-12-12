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


#endif // DMCLK_PORT_H