#ifndef DMCLK_PORT_H
#define DMCLK_PORT_H

#include "dmod.h"

/**
 * @brief Clock frequency type in Hz
 */
typedef uint64_t dmclk_frequency_t;

/**
 * @brief Time type in microseconds
 */
typedef uint64_t dmclk_time_us_t;

/**
 * @brief Port API macro for dmclk module
 * 
 * This macro is used to declare port-specific API functions
 * when building as a DMOD module.
 */
#if DMOD_MODULE_EN == ON
#define dmod_dmclk_port_api(VERSION, RET, NAME, PARAMS) \
    DMOD_INPUT_API(dmclk_port, VERSION, RET, NAME, PARAMS)
#else
#define dmod_dmclk_port_api(VERSION, RET, NAME, PARAMS) \
    extern RET dmclk_port##NAME PARAMS;
#endif

dmod_dmclk_port_api(1.0, int, _configure_internal, ( dmclk_frequency_t target_freq, dmclk_frequency_t tolerance) );
dmod_dmclk_port_api(1.0, int, _configure_external, ( dmclk_frequency_t target_freq, dmclk_frequency_t tolerance, dmclk_frequency_t oscillator_freq) );
dmod_dmclk_port_api(1.0, int, _configure_hibernatation, ( dmclk_frequency_t target_freq, dmclk_frequency_t tolerance, dmclk_frequency_t oscillator_freq) );
dmod_dmclk_port_api(1.0, void, _delay_us, ( dmclk_time_us_t time_us) );
dmod_dmclk_port_api(1.0, dmclk_frequency_t, _get_current_frequency, ( void ) );


#endif // DMCLK_PORT_H