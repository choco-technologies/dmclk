# DMCLK Examples

This document provides practical examples of using the DMCLK module in various scenarios.

## Example 1: Basic Clock Configuration

Initialize the system clock using a simple configuration file.

### Configuration File (config.ini)

```ini
[dmclk]
source=external
target_frequency=84000000
tolerance=1000
oscillator_frequency=8000000
```

### Code

```c
#include "dmclk.h"
#include "dmdrvi.h"
#include "dmini.h"
#include <stdio.h>

int main(void)
{
    // Load configuration
    dmini_context_t config = dmini_load("config.ini");
    if (config == NULL) {
        printf("Failed to load configuration\n");
        return -1;
    }

    // Create clock device
    dmdrvi_dev_num_t dev_num = {0};
    dmdrvi_context_t ctx = dmclk_dmdrvi_create(config, &dev_num);
    if (ctx == NULL) {
        printf("Failed to create clock device\n");
        dmini_free(config);
        return -1;
    }

    printf("Clock configured successfully\n");

    // Cleanup
    dmclk_dmdrvi_free(ctx);
    dmini_free(config);
    return 0;
}
```

## Example 2: Reading Clock Information

Query current clock configuration and frequency.

```c
#include "dmclk.h"
#include "dmdrvi.h"
#include "dmini.h"
#include <stdio.h>

void display_clock_info(dmdrvi_context_t ctx, void* handle)
{
    // Read formatted string
    char buffer[256];
    dmclk_dmdrvi_read(ctx, handle, buffer, sizeof(buffer));
    printf("Clock info: %s\n", buffer);

    // Get individual parameters
    dmclk_frequency_t freq, target, tolerance, osc_freq;
    dmclk_source_t source;

    dmclk_dmdrvi_ioctl(ctx, handle, dmclk_ioctl_cmd_get_frequency, &freq);
    dmclk_dmdrvi_ioctl(ctx, handle, dmclk_ioctl_cmd_get_target_frequency, &target);
    dmclk_dmdrvi_ioctl(ctx, handle, dmclk_ioctl_cmd_get_tolerance, &tolerance);
    dmclk_dmdrvi_ioctl(ctx, handle, dmclk_ioctl_cmd_get_oscillator_frequency, &osc_freq);
    dmclk_dmdrvi_ioctl(ctx, handle, dmclk_ioctl_cmd_get_source, &source);

    printf("Actual frequency: %lu Hz\n", freq);
    printf("Target frequency: %lu Hz\n", target);
    printf("Tolerance: %lu Hz\n", tolerance);
    printf("Oscillator frequency: %lu Hz\n", osc_freq);
    printf("Clock source: %d\n", source);
}

int main(void)
{
    dmini_context_t config = dmini_load("config.ini");
    dmdrvi_dev_num_t dev_num = {0};
    dmdrvi_context_t ctx = dmclk_dmdrvi_create(config, &dev_num);
    
    void* handle = dmclk_dmdrvi_open(ctx, DMDRVI_O_RDONLY);
    if (handle != NULL) {
        display_clock_info(ctx, handle);
        dmclk_dmdrvi_close(ctx, handle);
    }

    dmclk_dmdrvi_free(ctx);
    dmini_free(config);
    return 0;
}
```

## Example 3: Runtime Frequency Change

Change the system clock frequency at runtime.

```c
#include "dmclk.h"
#include "dmdrvi.h"
#include "dmini.h"
#include <stdio.h>

int change_frequency(dmdrvi_context_t ctx, void* handle, dmclk_frequency_t new_freq)
{
    dmclk_frequency_t old_freq;
    
    // Get current frequency
    dmclk_dmdrvi_ioctl(ctx, handle, dmclk_ioctl_cmd_get_frequency, &old_freq);
    printf("Current frequency: %lu Hz\n", old_freq);

    // Change to new frequency
    int ret = dmclk_dmdrvi_ioctl(ctx, handle, dmclk_ioctl_cmd_set_target_frequency, &new_freq);
    if (ret != 0) {
        printf("Failed to set frequency: %d\n", ret);
        return ret;
    }

    // Verify new frequency
    dmclk_dmdrvi_ioctl(ctx, handle, dmclk_ioctl_cmd_get_frequency, &new_freq);
    printf("New frequency: %lu Hz\n", new_freq);

    return 0;
}

int main(void)
{
    dmini_context_t config = dmini_load("config.ini");
    dmdrvi_dev_num_t dev_num = {0};
    dmdrvi_context_t ctx = dmclk_dmdrvi_create(config, &dev_num);
    void* handle = dmclk_dmdrvi_open(ctx, DMDRVI_O_RDONLY);

    // Change to 100 MHz
    change_frequency(ctx, handle, 100000000);

    // Change to 168 MHz
    change_frequency(ctx, handle, 168000000);

    dmclk_dmdrvi_close(ctx, handle);
    dmclk_dmdrvi_free(ctx);
    dmini_free(config);
    return 0;
}
```

## Example 4: Switching Clock Sources

Switch between internal and external clock sources.

```c
#include "dmclk.h"
#include "dmdrvi.h"
#include "dmini.h"
#include <stdio.h>

int switch_to_internal(dmdrvi_context_t ctx, void* handle)
{
    dmclk_source_t source = dmclk_source_internal;
    dmclk_frequency_t target = 16000000;  // 16 MHz internal RC

    printf("Switching to internal clock...\n");

    // Set clock source
    int ret = dmclk_dmdrvi_ioctl(ctx, handle, dmclk_ioctl_cmd_set_source, &source);
    if (ret != 0) {
        printf("Failed to set source: %d\n", ret);
        return ret;
    }

    // Set appropriate target frequency for internal clock
    ret = dmclk_dmdrvi_ioctl(ctx, handle, dmclk_ioctl_cmd_set_target_frequency, &target);
    if (ret != 0) {
        printf("Failed to set target frequency: %d\n", ret);
        return ret;
    }

    printf("Switched to internal clock\n");
    return 0;
}

int switch_to_external(dmdrvi_context_t ctx, void* handle, dmclk_frequency_t osc_freq, dmclk_frequency_t target_freq)
{
    dmclk_source_t source = dmclk_source_external;

    printf("Switching to external clock...\n");

    // Set oscillator frequency first
    int ret = dmclk_dmdrvi_ioctl(ctx, handle, dmclk_ioctl_cmd_set_oscillator_frequency, &osc_freq);
    if (ret != 0) {
        printf("Failed to set oscillator frequency: %d\n", ret);
        return ret;
    }

    // Set clock source
    ret = dmclk_dmdrvi_ioctl(ctx, handle, dmclk_ioctl_cmd_set_source, &source);
    if (ret != 0) {
        printf("Failed to set source: %d\n", ret);
        return ret;
    }

    // Set target frequency
    ret = dmclk_dmdrvi_ioctl(ctx, handle, dmclk_ioctl_cmd_set_target_frequency, &target_freq);
    if (ret != 0) {
        printf("Failed to set target frequency: %d\n", ret);
        return ret;
    }

    printf("Switched to external clock\n");
    return 0;
}

int main(void)
{
    dmini_context_t config = dmini_load("config.ini");
    dmdrvi_dev_num_t dev_num = {0};
    dmdrvi_context_t ctx = dmclk_dmdrvi_create(config, &dev_num);
    void* handle = dmclk_dmdrvi_open(ctx, DMDRVI_O_RDONLY);

    // Switch to internal clock
    switch_to_internal(ctx, handle);

    // Wait some time...
    dmclk_port_delay_us(1000000);  // 1 second

    // Switch back to external clock
    switch_to_external(ctx, handle, 8000000, 84000000);

    dmclk_dmdrvi_close(ctx, handle);
    dmclk_dmdrvi_free(ctx);
    dmini_free(config);
    return 0;
}
```

## Example 5: Programmatic Configuration

Create configuration programmatically without a file.

```c
#include "dmclk.h"
#include "dmdrvi.h"
#include "dmini.h"
#include <stdio.h>

dmini_context_t create_clock_config(const char* source, 
                                     dmclk_frequency_t target_freq,
                                     dmclk_frequency_t tolerance,
                                     dmclk_frequency_t osc_freq)
{
    dmini_context_t config = dmini_create();
    if (config == NULL) {
        return NULL;
    }

    dmini_set_string(config, "dmclk", "source", source);
    dmini_set_int(config, "dmclk", "target_frequency", target_freq);
    dmini_set_int(config, "dmclk", "tolerance", tolerance);
    
    if (osc_freq > 0) {
        dmini_set_int(config, "dmclk", "oscillator_frequency", osc_freq);
    }

    return config;
}

int main(void)
{
    // Create configuration for 100 MHz using 8 MHz crystal
    dmini_context_t config = create_clock_config("external", 100000000, 1000, 8000000);
    if (config == NULL) {
        printf("Failed to create configuration\n");
        return -1;
    }

    // Create device
    dmdrvi_dev_num_t dev_num = {0};
    dmdrvi_context_t ctx = dmclk_dmdrvi_create(config, &dev_num);
    if (ctx == NULL) {
        printf("Failed to create clock device\n");
        dmini_free(config);
        return -1;
    }

    printf("Clock configured to 100 MHz\n");

    dmclk_dmdrvi_free(ctx);
    dmini_free(config);
    return 0;
}
```

## Example 6: Error Handling

Robust error handling for clock configuration.

```c
#include "dmclk.h"
#include "dmdrvi.h"
#include "dmini.h"
#include <stdio.h>
#include <errno.h>

const char* error_to_string(int error)
{
    switch (-error) {
        case EINVAL: return "Invalid parameter";
        case ENOMEM: return "Out of memory";
        case ERANGE: return "Frequency out of range";
        case EIO: return "Hardware error";
        default: return "Unknown error";
    }
}

int configure_clock_with_validation(const char* config_file)
{
    dmini_context_t config = dmini_load(config_file);
    if (config == NULL) {
        printf("Error: Cannot load configuration file '%s'\n", config_file);
        return -1;
    }

    dmdrvi_dev_num_t dev_num = {0};
    dmdrvi_context_t ctx = dmclk_dmdrvi_create(config, &dev_num);
    if (ctx == NULL) {
        printf("Error: Cannot create clock device\n");
        printf("Check configuration parameters:\n");
        printf("  - source must be 'internal', 'external', or 'hibernation'\n");
        printf("  - target_frequency must be > 0\n");
        printf("  - tolerance must be > 0\n");
        printf("  - oscillator_frequency must be set for external/hibernation\n");
        dmini_free(config);
        return -1;
    }

    void* handle = dmclk_dmdrvi_open(ctx, DMDRVI_O_RDONLY);
    if (handle == NULL) {
        printf("Error: Cannot open clock device\n");
        dmclk_dmdrvi_free(ctx);
        dmini_free(config);
        return -1;
    }

    // Verify achieved frequency
    dmclk_frequency_t target, actual, tolerance;
    dmclk_dmdrvi_ioctl(ctx, handle, dmclk_ioctl_cmd_get_target_frequency, &target);
    dmclk_dmdrvi_ioctl(ctx, handle, dmclk_ioctl_cmd_get_frequency, &actual);
    dmclk_dmdrvi_ioctl(ctx, handle, dmclk_ioctl_cmd_get_tolerance, &tolerance);

    printf("Target frequency: %lu Hz\n", target);
    printf("Actual frequency: %lu Hz\n", actual);
    printf("Tolerance: ±%lu Hz\n", tolerance);

    int64_t deviation = (int64_t)actual - (int64_t)target;
    if (deviation < 0) deviation = -deviation;

    if ((uint64_t)deviation > tolerance) {
        printf("Warning: Frequency deviation (%ld Hz) exceeds tolerance\n", deviation);
    } else {
        printf("Clock configured successfully\n");
    }

    dmclk_dmdrvi_close(ctx, handle);
    dmclk_dmdrvi_free(ctx);
    dmini_free(config);
    return 0;
}

int main(void)
{
    int ret = configure_clock_with_validation("config.ini");
    return ret;
}
```

## Example 7: Power Management

Dynamically adjust clock frequency based on system load.

```c
#include "dmclk.h"
#include "dmdrvi.h"
#include "dmini.h"
#include <stdio.h>

typedef enum {
    POWER_MODE_LOW,      // 16 MHz
    POWER_MODE_MEDIUM,   // 84 MHz
    POWER_MODE_HIGH,     // 168 MHz
} power_mode_t;

int set_power_mode(dmdrvi_context_t ctx, void* handle, power_mode_t mode)
{
    dmclk_frequency_t freq;
    const char* mode_name;

    switch (mode) {
        case POWER_MODE_LOW:
            freq = 16000000;
            mode_name = "LOW";
            break;
        case POWER_MODE_MEDIUM:
            freq = 84000000;
            mode_name = "MEDIUM";
            break;
        case POWER_MODE_HIGH:
            freq = 168000000;
            mode_name = "HIGH";
            break;
        default:
            return -1;
    }

    printf("Setting power mode to %s (%lu Hz)...\n", mode_name, freq);

    int ret = dmclk_dmdrvi_ioctl(ctx, handle, dmclk_ioctl_cmd_set_target_frequency, &freq);
    if (ret == 0) {
        printf("Power mode changed successfully\n");
    } else {
        printf("Failed to change power mode: %d\n", ret);
    }

    return ret;
}

int main(void)
{
    dmini_context_t config = dmini_load("config.ini");
    dmdrvi_dev_num_t dev_num = {0};
    dmdrvi_context_t ctx = dmclk_dmdrvi_create(config, &dev_num);
    void* handle = dmclk_dmdrvi_open(ctx, DMDRVI_O_RDONLY);

    // Start in low power mode
    set_power_mode(ctx, handle, POWER_MODE_LOW);

    // Simulate load increase
    printf("Load increased, switching to medium power...\n");
    dmclk_port_delay_us(1000000);
    set_power_mode(ctx, handle, POWER_MODE_MEDIUM);

    // Simulate high load
    printf("High load detected, switching to high power...\n");
    dmclk_port_delay_us(1000000);
    set_power_mode(ctx, handle, POWER_MODE_HIGH);

    // Back to low power
    printf("Load decreased, switching back to low power...\n");
    dmclk_port_delay_us(1000000);
    set_power_mode(ctx, handle, POWER_MODE_LOW);

    dmclk_dmdrvi_close(ctx, handle);
    dmclk_dmdrvi_free(ctx);
    dmini_free(config);
    return 0;
}
```

## See Also

- [API Reference](api-reference.md) - Complete API documentation
- [Configuration Guide](configuration.md) - Detailed configuration options
- [Main Documentation](dmclk.md) - Module overview
