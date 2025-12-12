#ifndef DMCLK_PORT_DEFS_H
#define DMCLK_PORT_DEFS_H

/**
 * @brief Port-specific definitions for dmclk module
 * 
 * This file is intentionally minimal to allow for easy portability
 * across different microcontroller families.
 */

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

#endif // DMCLK_PORT_DEFS_H
