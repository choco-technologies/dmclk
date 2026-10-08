#ifndef DMCLK_DOMAIN_H
#define DMCLK_DOMAIN_H

#include "dmclk_port.h"

/* Internal port hooks. Called with all clock mutations serialized. Acquire
 * must be idempotent for a domain; release is called only for its last lease. */
int dmclk_domain_acquire_hardware(dmclk_domain_t domain, dmclk_frequency_t target,
                                  dmclk_frequency_t tolerance, dmclk_frequency_t *actual);
int dmclk_domain_release_hardware(dmclk_domain_t domain);

/* Protect legacy reconfiguration (and deinit) from active reservations. */
int dmclk_domain_begin_configuration(void);
void dmclk_domain_end_configuration(void);

static inline int dmclk_domain_matches(dmclk_frequency_t actual,
                                       dmclk_frequency_t target,
                                       dmclk_frequency_t tolerance)
{
    return actual != 0 && (actual > target ? actual - target : target - actual) <= tolerance;
}

#endif
