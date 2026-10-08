#include "domain.h"
#include <errno.h>

static uint32_t references[dmclk_domain_sai2 + 1];
static int busy;

static int begin_operation(int require_unreserved)
{
    int rc = 0;
    Dmod_EnterCritical();
    if (busy) {
        rc = -EBUSY;
    } else if (require_unreserved) {
        for (unsigned i = 0; i < sizeof(references) / sizeof(references[0]); ++i) {
            if (references[i]) {
                rc = -EBUSY;
                break;
            }
        }
    }
    if (rc == 0) {
        busy = 1;
    }
    Dmod_ExitCritical();
    return rc;
}

int dmclk_domain_begin_configuration(void)
{
    return begin_operation(1);
}

void dmclk_domain_end_configuration(void)
{
    Dmod_EnterCritical();
    busy = 0;
    Dmod_ExitCritical();
}

dmod_dmclk_port_api_declaration(1.0, int, _acquire_domain, (dmclk_domain_t domain,
    dmclk_frequency_t target_hz, dmclk_frequency_t tolerance_hz, dmclk_frequency_t *actual_hz))
{
    if ((unsigned)domain >= sizeof(references) / sizeof(references[0]) ||
        target_hz == 0 || actual_hz == NULL) {
        return -EINVAL;
    }
    int rc = begin_operation(0);
    if (rc != 0) {
        return rc;
    }
    dmclk_frequency_t actual = 0;
    if (references[domain] == UINT32_MAX) {
        rc = -EOVERFLOW;
    } else if (references[domain]) {
        actual = dmclk_port_get_domain_frequency(domain);
        rc = dmclk_domain_matches(actual, target_hz, tolerance_hz) ? 0 : -EBUSY;
        if (rc == 0) {
            /* The backend also checks sub-Hz error before rounding readback. */
            rc = dmclk_domain_acquire_hardware(domain, target_hz, tolerance_hz, &actual);
        }
    } else {
        rc = dmclk_domain_acquire_hardware(domain, target_hz, tolerance_hz, &actual);
    }
    if (rc == 0) {
        ++references[domain];
        *actual_hz = actual;
    }
    dmclk_domain_end_configuration();
    return rc;
}

dmod_dmclk_port_api_declaration(1.0, int, _release_domain, (dmclk_domain_t domain))
{
    if ((unsigned)domain >= sizeof(references) / sizeof(references[0])) {
        return -EINVAL;
    }
    int rc = begin_operation(0);
    if (rc != 0) {
        return rc;
    }
    if (references[domain] == 0) {
        rc = -EINVAL;
    } else {
        if (references[domain] == 1) {
            rc = dmclk_domain_release_hardware(domain);
        }
        if (rc == 0) {
            --references[domain];
        }
    }
    dmclk_domain_end_configuration();
    return rc;
}
