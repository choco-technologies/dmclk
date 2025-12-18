#define DMOD_ENABLE_REGISTRATION
#include "dmod.h"
#include "dmdrvi.h"
#include "dmini.h"
#include "dmclk.h"
#include <stdio.h>

/**
 * @brief Test application to verify dmclk functionality in dmod-boot
 */

// Configuration string for dmclk
static const char* dmclk_config = 
    "[dmclk]\n"
    "source=external\n"
    "target_frequency=216000000\n"
    "tolerance=1000\n"
    "oscillator_frequency=25000000\n";

static void print_clock_info(dmdrvi_context_t clk_ctx, void* handle)
{
    dmclk_frequency_t freq;
    dmclk_source_t source;
    dmclk_frequency_t tolerance;
    dmclk_frequency_t osc_freq;
    dmclk_frequency_t target_freq;
    
    // Get current frequency
    if (dmclk_dmdrvi_ioctl(clk_ctx, handle, dmclk_ioctl_cmd_get_frequency, &freq) == 0)
    {
        Dmod_Printf("Current frequency: %u Hz (%u MHz)\n", (unsigned int)freq, (unsigned int)(freq / 1000000));
    }
    else
    {
        Dmod_Printf("Failed to get current frequency\n");
    }
    
    // Get clock source
    if (dmclk_dmdrvi_ioctl(clk_ctx, handle, dmclk_ioctl_cmd_get_source, &source) == 0)
    {
        const char* source_str = "unknown";
        switch (source)
        {
            case dmclk_source_internal:
                source_str = "internal";
                break;
            case dmclk_source_external:
                source_str = "external";
                break;
            case dmclk_source_hibernation:
                source_str = "hibernation";
                break;
            default:
                break;
        }
        Dmod_Printf("Clock source: %s\n", source_str);
    }
    else
    {
        Dmod_Printf("Failed to get clock source\n");
    }
    
    // Get target frequency
    if (dmclk_dmdrvi_ioctl(clk_ctx, handle, dmclk_ioctl_cmd_get_target_frequency, &target_freq) == 0)
    {
        Dmod_Printf("Target frequency: %u Hz (%u MHz)\n", (unsigned int)target_freq, (unsigned int)(target_freq / 1000000));
    }
    else
    {
        Dmod_Printf("Failed to get target frequency\n");
    }
    
    // Get tolerance
    if (dmclk_dmdrvi_ioctl(clk_ctx, handle, dmclk_ioctl_cmd_get_tolerance, &tolerance) == 0)
    {
        Dmod_Printf("Tolerance: %u Hz\n", (unsigned int)tolerance);
    }
    else
    {
        Dmod_Printf("Failed to get tolerance\n");
    }
    
    // Get oscillator frequency
    if (dmclk_dmdrvi_ioctl(clk_ctx, handle, dmclk_ioctl_cmd_get_oscillator_frequency, &osc_freq) == 0)
    {
        Dmod_Printf("Oscillator frequency: %u Hz (%u MHz)\n", (unsigned int)osc_freq, (unsigned int)(osc_freq / 1000000));
    }
    else
    {
        Dmod_Printf("Failed to get oscillator frequency\n");
    }
    
    // Read clock information as string
    char buffer[256];
    size_t bytes_read = dmclk_dmdrvi_read(clk_ctx, handle, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0)
    {
        buffer[bytes_read] = '\0';
        Dmod_Printf("Clock info string: %s\n", buffer);
    }
}

int test_dmclk_main(int argc, char* argv[])
{
    Dmod_Printf("\n=== DMCLK Test Application ===\n\n");
    
    // Create a temporary file with configuration
    Dmod_Printf("Creating configuration...\n");
    dmini_context_t config = dmini_loads(dmclk_config);
    if (config == NULL)
    {
        Dmod_Printf("ERROR: Failed to create configuration\n");
        return -1;
    }
    
    Dmod_Printf("Creating dmclk device...\n");
    dmdrvi_dev_num_t dev_num = {0};
    dmdrvi_context_t clk_ctx = dmclk_dmdrvi_create(config, &dev_num);
    if (clk_ctx == NULL)
    {
        Dmod_Printf("ERROR: Failed to create dmclk device\n");
        dmini_free(config);
        return -1;
    }
    
    Dmod_Printf("Opening dmclk device...\n");
    void* handle = dmclk_dmdrvi_open(clk_ctx, DMDRVI_O_RDONLY);
    if (handle == NULL)
    {
        Dmod_Printf("ERROR: Failed to open dmclk device\n");
        dmclk_dmdrvi_free(clk_ctx);
        dmini_free(config);
        return -1;
    }
    
    Dmod_Printf("\n--- Clock Configuration ---\n");
    print_clock_info(clk_ctx, handle);
    
    // Check if the actual frequency is within tolerance
    dmclk_frequency_t actual_freq, target_freq, tolerance;
    if (dmclk_dmdrvi_ioctl(clk_ctx, handle, dmclk_ioctl_cmd_get_frequency, &actual_freq) == 0 &&
        dmclk_dmdrvi_ioctl(clk_ctx, handle, dmclk_ioctl_cmd_get_target_frequency, &target_freq) == 0 &&
        dmclk_dmdrvi_ioctl(clk_ctx, handle, dmclk_ioctl_cmd_get_tolerance, &tolerance) == 0)
    {
        int64_t diff = (int64_t)actual_freq - (int64_t)target_freq;
        if (diff < 0) diff = -diff;
        
        Dmod_Printf("\n--- Verification ---\n");
        Dmod_Printf("Target: %u Hz, Actual: %u Hz, Difference: %d Hz, Tolerance: %u Hz\n",
                   (unsigned int)target_freq, (unsigned int)actual_freq, (int)diff, (unsigned int)tolerance);
        
        if (diff <= tolerance)
        {
            Dmod_Printf("✓ Clock is configured within tolerance\n");
        }
        else
        {
            Dmod_Printf("✗ Clock is NOT within tolerance (difference: %d Hz > %u Hz)\n", 
                       (int)diff, (unsigned int)tolerance);
        }
    }
    
    Dmod_Printf("\n--- Test: Change clock to internal 16 MHz ---\n");
    dmclk_frequency_t new_target = 16000000;
    dmclk_source_t new_source = dmclk_source_internal;
    
    if (dmclk_dmdrvi_ioctl(clk_ctx, handle, dmclk_ioctl_cmd_set_source, &new_source) == 0)
    {
        Dmod_Printf("Changed source to internal\n");
    }
    else
    {
        Dmod_Printf("Failed to change source\n");
    }
    
    if (dmclk_dmdrvi_ioctl(clk_ctx, handle, dmclk_ioctl_cmd_set_target_frequency, &new_target) == 0)
    {
        Dmod_Printf("Changed target frequency to 16 MHz\n");
        print_clock_info(clk_ctx, handle);
    }
    else
    {
        Dmod_Printf("Failed to change target frequency\n");
    }
    
    Dmod_Printf("\n--- Cleanup ---\n");
    dmclk_dmdrvi_close(clk_ctx, handle);
    dmclk_dmdrvi_free(clk_ctx);
    dmini_free(config);
    
    Dmod_Printf("\n=== Test Complete ===\n\n");
    return 0;
}

DMOD_MAIN(test_dmclk, test_dmclk_main)
