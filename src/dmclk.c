#define DMOD_ENABLE_REGISTRATION    ON
#include "dmclk.h"
#include "dmdrvi.h"
#include "dmini.h"
#include "dmclk_port.h"

/**
 * @brief Initialize the DMDRVI module
 * 
 * @param Config Pointer to Dmod_Config_t structure with configuration parameters
 * 
 * @return int 0 on success, non-zero on failure
 */
int dmod_init(const Dmod_Config_t *Config)
{
    Dmod_Printf("DMDRVI interface module initialized\n");
    return 0;
}

/**
 * @brief Deinitialize the DMDRVI module
 * 
 * @return int 0 on success, non-zero on failure
 */
int dmod_deinit(void)
{
    Dmod_Printf("DMDRVI interface module deinitialized\n");
    return 0;
}