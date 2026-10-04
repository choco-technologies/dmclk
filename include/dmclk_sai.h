#ifndef DMCLK_SAI_H
#define DMCLK_SAI_H
#include "dmod.h"
#include "dmclk_defs.h"
#include "dmclk_types.h"
/* Module API for peripheral drivers. See dmclk_port.h for clock semantics. */
dmod_dmclk_api(1.0, int, _sai_acquire, ( dmclk_domain_t domain, dmclk_frequency_t target, dmclk_frequency_t tolerance, dmclk_frequency_t *actual ) );
dmod_dmclk_api(1.0, int, _sai_release, ( dmclk_domain_t domain ) );

#endif
