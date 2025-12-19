#define DMOD_ENABLE_REGISTRATION    ON
#include "dmclk.h"
#include "dmdrvi.h"
#include "dmini.h"
#include "dmclk_port.h"
#include <errno.h>
#include <string.h>

// Magic set to DCLK
#define DMCLK_CONTEXT_MAGIC    0x44434C4B

/**
 * @brief Configuration structure
 */
struct config
{
    dmclk_frequency_t target_frequency;
    dmclk_frequency_t tolerance;
    dmclk_frequency_t oscillator_frequency;
    dmclk_source_t source;
};

/**
 * @brief DMDRVI context structure
 */
struct dmdrvi_context
{
    uint32_t magic;                    /**< Magic number for validation */
    struct config config;              /**< Configuration parameters */
    dmclk_frequency_t current_frequency;  /**< Current clock frequency in Hz */
};

/**
 * @brief Validate DMDRVI context
 * 
 * @param context DMDRVI context to validate
 * 
 * @return int 1 if valid, 0 otherwise
 */
static int is_valid_context(dmdrvi_context_t context)
{
    return (context != NULL && context->magic == DMCLK_CONTEXT_MAGIC);
}

/**
 * @brief Convert clock source enum to string
 * 
 * @param source Clock source
 * 
 * @return const char* String representation of clock source
 */
static const char* source_to_string(dmclk_source_t source)
{
    switch (source)
    {
        case dmclk_source_internal:
            return "internal";
        case dmclk_source_external:
            return "external";
        case dmclk_source_hibernation:
            return "hibernation";
        default:
            return "unknown";
    }
}

/**
 * @brief Convert string to clock source enum
 * 
 * @param source_str String representation of clock source
 * 
 * @return dmclk_source_t Clock source enum
 */
static dmclk_source_t string_to_source(const char* source_str)
{
    if (source_str != NULL)
    {
        if (strcmp(source_str, "internal") == 0)
        {
            return dmclk_source_internal;
        }
        else if (strcmp(source_str, "external") == 0)
        {
            return dmclk_source_external;
        }
        else if (strcmp(source_str, "hibernation") == 0)
        {
            return dmclk_source_hibernation;
        }
    }
    return dmclk_source_unkown;
}

/**
 * @brief Check configuration parameters
 * 
 * @param context DMDRVI context
 * 
 * @return int 0 if valid, non-zero otherwise
 */
static int check_config_parameters(struct config* cfg)
{
    if (cfg->target_frequency == 0)
    {
        DMOD_LOG_ERROR("Target frequency not set in configuration\n");
        return -EINVAL;
    }
    else if (cfg->tolerance == 0)
    {
        DMOD_LOG_ERROR("Tolerance not set in configuration\n");
        return -EINVAL;
    }
    else if (cfg->source == dmclk_source_unkown)
    {
        DMOD_LOG_ERROR("Clock source not set or unknown in configuration\n");
        return -EINVAL;
    }
    else if(cfg->source != dmclk_source_internal && cfg->oscillator_frequency == 0)
    {
        DMOD_LOG_ERROR("Oscillator frequency not set in configuration for external or hibernation source\n");
        return -EINVAL;
    }
    return 0;
}

/**
 * @brief Read configuration parameters from Dmini context
 * 
 * @param context DMDRVI context
 * @param config Dmini context with configuration data
 * 
 * @return int 0 on success, non-zero on failure
 */
static int read_config_parameters(dmdrvi_context_t context, dmini_context_t config)
{   
    context->config.target_frequency = (dmclk_frequency_t)dmini_get_int(config, "dmclk", "target_frequency", 0);
    context->config.tolerance = (dmclk_frequency_t)dmini_get_int(config, "dmclk", "tolerance", 0);
    context->config.oscillator_frequency = (dmclk_frequency_t)dmini_get_int(config, "dmclk", "oscillator_frequency", 0);
    context->config.source = string_to_source(dmini_get_string(config, "dmclk", "source", NULL));
    
    return check_config_parameters(&context->config);
}

/**
 * @brief Configure the clock based on context parameters
 * 
 * @param context DMDRVI context
 * 
 * @return int 0 on success, non-zero on failure
 */
static int configure(dmdrvi_context_t context)
{
    int ret = -1;
    switch (context->config.source)
    {
        case dmclk_source_internal:
            ret = dmclk_port_configure_internal(context->config.target_frequency, context->config.tolerance);
            break;
        case dmclk_source_external:
            ret = dmclk_port_configure_external(context->config.target_frequency, context->config.tolerance, context->config.oscillator_frequency);
            break;
        case dmclk_source_hibernation:
            ret = dmclk_port_configure_hibernatation(context->config.target_frequency, context->config.tolerance, context->config.oscillator_frequency);
            break;
        default:
            DMOD_LOG_ERROR("Unknown clock source in configuration\n");
            ret = -EINVAL;
            break;
    }
    if (ret == 0)
    {
        context->current_frequency = dmclk_port_get_current_frequency();
    }
    return ret;
}

/**
 * @brief Update configuration parameters in context
 *
 * @param cfg Configuration structure to update
 * @param command IOCTL command
 * @param arg Argument for the command
 * 
 * @return int 0 on success, non-zero on failure
 */
static int update_configuration(struct config* cfg, int command, void* arg)
{
    int ret = 0;
    switch (command)
    {
        case dmclk_ioctl_cmd_set_target_frequency:
            cfg->target_frequency = *(dmclk_frequency_t*)arg;
            break;
        case dmclk_ioctl_cmd_set_tolerance:
            cfg->tolerance = *(dmclk_frequency_t*)arg;
            break;
        case dmclk_ioctl_cmd_set_oscillator_frequency:
            cfg->oscillator_frequency = *(dmclk_frequency_t*)arg;
            break;
        case dmclk_ioctl_cmd_set_source:
            cfg->source = *(dmclk_source_t*)arg;
            break;
        default:
            DMOD_LOG_ERROR("Invalid configuration command %d in update_configuration\n", command);
            ret = -EINVAL;
            break;
    }
    if (ret == 0)
    {
        ret = check_config_parameters(cfg);
    }
    return ret;
}

/**
 * @brief Read configuration parameters from context
 * 
 * @param context DMDRVI context
 * @param command IOCTL command
 * @param arg Argument for the command
 * 
 * @return int 0 on success, non-zero on failure
 */
static int read_configuration(dmdrvi_context_t context, int command, void* arg)
{
    int ret = 0;
    switch (command)
    {
        case dmclk_ioctl_cmd_get_target_frequency:
            *(dmclk_frequency_t*)arg = context->config.target_frequency;
            break;
        case dmclk_ioctl_cmd_get_tolerance:
            *(dmclk_frequency_t*)arg = context->config.tolerance;
            break;
        case dmclk_ioctl_cmd_get_oscillator_frequency:
            *(dmclk_frequency_t*)arg = context->config.oscillator_frequency;
            break;
        case dmclk_ioctl_cmd_get_source:
            *(dmclk_source_t*)arg = context->config.source;
            break;
        case dmclk_ioctl_cmd_get_frequency:
            *(dmclk_frequency_t*)arg = context->current_frequency;
            break;
        default:
            DMOD_LOG_ERROR("Invalid configuration command %d in read_configuration\n", command);
            ret = -EINVAL;
            break;
    }
    return ret;
}

/**
 * @brief Initialize the DMDRVI module
 * 
 * @param Config Pointer to Dmod_Config_t structure with configuration parameters
 * 
 * @return int 0 on success, non-zero on failure
 */
int dmod_init(const Dmod_Config_t *Config)
{
    DMOD_LOG_INFO("DMDRVI interface module initialized\n");
    return 0;
}

/**
 * @brief Deinitialize the DMDRVI module
 * 
 * @return int 0 on success, non-zero on failure
 */
int dmod_deinit(void)
{
    DMOD_LOG_INFO("DMDRVI interface module deinitialized\n");   
    return 0;
}

/**
 * @brief Create a new DMDRVI context
 * 
 * @param config Pointer to configuration data
 * @param dev_num Output pointer to device number structure - driver fills in major, minor, and flags
 * 
 * @return dmdrvi_context_t New DMDRVI context
 */
dmod_dmdrvi_dif_api_declaration(1.0, dmclk, dmdrvi_context_t, _create, ( dmini_context_t config, dmdrvi_dev_num_t* dev_num ))
{
    if(config == NULL || dev_num == NULL)
    {
        DMOD_LOG_ERROR("Invalid parameters to dmclk_dmdrvi_create\n");
        return NULL;
    }

    // Set device numbering - dmclk uses no numbering (device path: /dev/dmclk)
    dev_num->major = 0;
    dev_num->minor = 0;
    dev_num->flags = DMDRVI_NUM_NONE;

    dmdrvi_context_t context = Dmod_Malloc(sizeof(struct dmdrvi_context));
    if (context != NULL)
    {
        memset(context, 0, sizeof(*context));
        context->magic = DMCLK_CONTEXT_MAGIC;
        if (read_config_parameters(context, config) != 0
         || configure(context) != 0)
        {
            Dmod_Free(context);
            return NULL;
        }
        else 
        {
            DMOD_LOG_INFO("Clock configured to %lu Hz\n", context->current_frequency);
        }
    }
    return context;
}

/**
 * @brief Free the DMDRVI context
 * 
 */
dmod_dmdrvi_dif_api_declaration(1.0, dmclk, void, _free, ( dmdrvi_context_t context ))
{
    if (is_valid_context(context))
    {
        context->magic = 0; // Invalidate context
        Dmod_Free(context);
    }
}

/**
 * @brief Open a device handle
 * 
 * @param context DMDRVI context
 * @param flags Open flags
 * 
 * @return void* Device handle
 */
dmod_dmdrvi_dif_api_declaration(1.0, dmclk, void*, _open, ( dmdrvi_context_t context, int flags ))
{
    if(!is_valid_context(context))
    {
        DMOD_LOG_ERROR("Invalid DMDRVI context in dmclk_dmdrvi_open\n");
        return NULL;
    }
    if(flags & DMDRVI_O_WRONLY)
    {
        DMOD_LOG_ERROR("Write access is not supported in dmclk_dmdrvi_open\n");
        return NULL;
    }
    return context;
}

/**
 * @brief Close a device handle
 * 
 * @param context DMDRVI context
 * @param handle Device handle
 * @return void
 */
dmod_dmdrvi_dif_api_declaration(1.0, dmclk, void, _close, ( dmdrvi_context_t context, void* handle ))
{
    // No specific action needed to close the clock device
}

/**
 * @brief Read from the device
 * 
 * The data is returned in the format:
 * "frequency=<current_frequency>;source=<source_string>;oscillator_frequency=<oscillator_frequency
 * 
 * @param context DMDRVI context
 * @param handle Device handle
 * @param buffer Buffer to read data into
 * @param size Size of the buffer
 * 
 * @return size_t Number of bytes read
 */
dmod_dmdrvi_dif_api_declaration(1.0, dmclk, size_t, _read, ( dmdrvi_context_t context, void* handle, void* buffer, size_t size ))
{
    int written = Dmod_SnPrintf(buffer, size, "frequency=%lu;source=%s;oscillator_frequency=%lu",
                  context->current_frequency,
                  source_to_string(context->config.source),
                  context->config.oscillator_frequency);
    return (written > 0) ? (size_t)written : 0;
}

/**
 * @brief Write to the device
 * 
 * @note This function is not implemented as the clock device is read-only.
 * 
 * @param context DMDRVI context
 * @param handle Device handle
 * @param buffer Buffer with data to write
 * @param size Number of bytes to write
 * 
 * @return size_t Number of bytes written
 */
dmod_dmdrvi_dif_api_declaration(1.0, dmclk, size_t, _write, ( dmdrvi_context_t context, void* handle, const void* buffer, size_t size ))
{
    // TODO: Implement _write function
    return 0;
}

/**
 * @brief Ioctl operation on the device
 * 
 * List of supported commands can be found in #dmclk_ioctl_cmd_t.
 * 
 * @param context DMDRVI context
 * @param handle Device handle
 * @param command Ioctl command
 * @param arg Argument for the command
 * 
 * @return int 0 on success, non-zero on failure
 */
dmod_dmdrvi_dif_api_declaration(1.0, dmclk, int, _ioctl, ( dmdrvi_context_t context, void* handle, int command, void* arg ))
{
    int ret = 0;
    if (!is_valid_context(context))
    {
        DMOD_LOG_ERROR("Invalid DMDRVI context in dmclk_dmdrvi_ioctl\n");
        return -EINVAL;
    }

    if(command >= dmclk_ioctl_cmd_max)
    {
        DMOD_LOG_ERROR("Invalid ioctl command %d in dmclk_dmdrvi_ioctl\n", command);
        return -EINVAL;
    }
    else if(command == dmclk_ioctl_cmd_reconfigure)
    {
        ret = configure(context);
        if (ret == 0)
        {
            DMOD_LOG_INFO("Clock reconfigured to %lu Hz\n", context->current_frequency);
        }
    }
    else if(arg == NULL)  
    {
        DMOD_LOG_ERROR("Null argument for ioctl command %d in dmclk_dmdrvi_ioctl\n", command);
        return -EINVAL;
    }
    else 
    {
        struct config new_config = {0};
        memcpy(&new_config, &context->config, sizeof(struct config));

        ret = read_configuration(context, command, arg); 
        if(ret != 0)
        {
            // Write operation
            ret = update_configuration(&new_config, command, arg);
            if (ret == 0)
            {
                // Apply new configuration
                memcpy(&context->config, &new_config, sizeof(struct config));
                ret = configure(context);
                if (ret == 0)
                {
                    DMOD_LOG_INFO("Clock reconfigured to %lu Hz\n", context->current_frequency);
                }
            }
        }
    }

    return ret;
}

/**
 * @brief Flush device buffers
 * 
 * @param context DMDRVI context
 * @param handle Device handle
 * 
 * @return int 0 on success, non-zero on failure
 */
dmod_dmdrvi_dif_api_declaration(1.0, dmclk, int, _flush, ( dmdrvi_context_t context, void* handle ))
{
    return 0;
}

/**
 * @brief Get device statistics
 * 
 * @param context DMDRVI context
 * @param handle Device handle
 * @param stat Pointer to dmdrvi_stat_t structure to fill
 * 
 * @return int 0 on success, non-zero on failure
 */
dmod_dmdrvi_dif_api_declaration(1.0, dmclk, int, _stat, ( dmdrvi_context_t context, void* handle, dmdrvi_stat_t* stat ))
{
    if(!is_valid_context(context) || stat == NULL)
    {
        DMOD_LOG_ERROR("Invalid parameters in dmclk_dmdrvi_stat\n");
        return -EINVAL;
    }

    char info_buffer[256];
    int result = dmdrvi_dmclk_read(context, handle, info_buffer, sizeof(info_buffer));
    if(result < 0)
    {
        return result;
    }
    stat->size = (uint32_t)result;
    stat->mode = 0444; // Read-only permissions
    return 0;
}