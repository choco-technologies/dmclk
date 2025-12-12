# DMCLK API Reference

## Types

### dmclk_frequency_t

```c
typedef uint64_t dmclk_frequency_t;
```

Represents clock frequency in Hertz (Hz). Uses 64-bit unsigned integer to support high-frequency clocks.

### dmclk_time_us_t

```c
typedef uint64_t dmclk_time_us_t;
```

Represents time in microseconds (µs). Used for delay operations.

### dmclk_source_t

```c
typedef enum 
{
    dmclk_source_unkown = 0,        /**< Unknown clock source */
    dmclk_source_internal,          /**< Internal RC oscillator */
    dmclk_source_external,          /**< External crystal or oscillator */
    dmclk_source_hibernation,       /**< Low-power hibernation clock source */
} dmclk_source_t;
```

Enumerates available clock sources.

### dmclk_ioctl_cmd_t

```c
typedef enum 
{
    dmclk_ioctl_cmd_get_frequency = 1,   /**< Get current clock frequency */
    dmclk_ioctl_cmd_set_source,          /**< Set clock source */
    dmclk_ioctl_cmd_get_source,          /**< Get clock source */
    dmclk_ioctl_cmd_set_tolerance,       /**< Set frequency tolerance */
    dmclk_ioctl_cmd_get_tolerance,       /**< Get frequency tolerance */
    dmclk_ioctl_cmd_set_oscillator_frequency, /**< Set oscillator frequency */
    dmclk_ioctl_cmd_get_oscillator_frequency, /**< Get oscillator frequency */
    dmclk_ioctl_cmd_set_target_frequency,    /**< Set target frequency */
    dmclk_ioctl_cmd_get_target_frequency,    /**< Get target frequency */
    dmclk_ioctl_cmd_reconfigure,             /**< Reconfigure clock with current settings */
    dmclk_ioctl_cmd_max
} dmclk_ioctl_cmd_t;
```

IOCTL commands for clock device control.

## DMDRVI Interface Functions

### dmclk_dmdrvi_create

```c
dmdrvi_context_t dmclk_dmdrvi_create(dmini_context_t config, const dmdrvi_dev_num_t* dev_num);
```

Creates and initializes a new DMCLK device context.

**Parameters:**
- `config`: DMINI configuration context with clock parameters
- `dev_num`: Device number structure (reserved for future use)

**Returns:**
- Valid context handle on success
- NULL on failure

**Configuration Requirements:**
The `config` parameter must contain a `[dmclk]` section with:
- `source`: Clock source string ("internal", "external", "hibernation")
- `target_frequency`: Target frequency in Hz
- `tolerance`: Acceptable frequency deviation in Hz
- `oscillator_frequency`: Oscillator frequency (required for external/hibernation sources)

**Example:**
```c
dmini_context_t config = dmini_load("config.ini");
dmdrvi_dev_num_t dev_num = {0};
dmdrvi_context_t ctx = dmclk_dmdrvi_create(config, &dev_num);
if (ctx == NULL) {
    // Handle error
}
```

### dmclk_dmdrvi_free

```c
void dmclk_dmdrvi_free(dmdrvi_context_t context);
```

Frees resources associated with a DMCLK device context.

**Parameters:**
- `context`: Device context to free

**Note:** Always call this function when done with a context to prevent memory leaks.

### dmclk_dmdrvi_open

```c
void* dmclk_dmdrvi_open(dmdrvi_context_t context, int flags);
```

Opens a clock device handle for operations.

**Parameters:**
- `context`: Device context from `dmclk_dmdrvi_create`
- `flags`: Open flags (DMDRVI_O_RDONLY supported, DMDRVI_O_WRONLY not supported)

**Returns:**
- Valid device handle on success
- NULL on failure

**Example:**
```c
void* handle = dmclk_dmdrvi_open(ctx, DMDRVI_O_RDONLY);
if (handle == NULL) {
    // Handle error
}
```

### dmclk_dmdrvi_close

```c
void dmclk_dmdrvi_close(dmdrvi_context_t context, void* handle);
```

Closes a device handle.

**Parameters:**
- `context`: Device context
- `handle`: Device handle to close

### dmclk_dmdrvi_read

```c
size_t dmclk_dmdrvi_read(dmdrvi_context_t context, void* handle, void* buffer, size_t size);
```

Reads current clock configuration as a formatted string.

**Parameters:**
- `context`: Device context
- `handle`: Device handle
- `buffer`: Buffer to receive data
- `size`: Size of buffer in bytes

**Returns:** Number of bytes written to buffer

**Output Format:**
```
frequency=<current_freq>;source=<source_name>;oscillator_frequency=<osc_freq>
```

**Example:**
```c
char buffer[256];
size_t bytes = dmclk_dmdrvi_read(ctx, handle, buffer, sizeof(buffer));
// buffer contains: "frequency=84000000;source=external;oscillator_frequency=8000000"
```

### dmclk_dmdrvi_write

```c
size_t dmclk_dmdrvi_write(dmdrvi_context_t context, void* handle, const void* buffer, size_t size);
```

Write operation - **not implemented**.

**Returns:** 0 (operation not supported)

### dmclk_dmdrvi_ioctl

```c
int dmclk_dmdrvi_ioctl(dmdrvi_context_t context, void* handle, int command, void* arg);
```

Performs control operations on the clock device.

**Parameters:**
- `context`: Device context
- `handle`: Device handle
- `command`: IOCTL command from `dmclk_ioctl_cmd_t`
- `arg`: Command-specific argument (pointer to data)

**Returns:**
- 0 on success
- Negative error code on failure

#### Get Commands

##### dmclk_ioctl_cmd_get_frequency

Gets the current actual clock frequency.

```c
dmclk_frequency_t freq;
int ret = dmclk_dmdrvi_ioctl(ctx, handle, dmclk_ioctl_cmd_get_frequency, &freq);
```

##### dmclk_ioctl_cmd_get_source

Gets the current clock source.

```c
dmclk_source_t source;
int ret = dmclk_dmdrvi_ioctl(ctx, handle, dmclk_ioctl_cmd_get_source, &source);
```

##### dmclk_ioctl_cmd_get_tolerance

Gets the frequency tolerance.

```c
dmclk_frequency_t tolerance;
int ret = dmclk_dmdrvi_ioctl(ctx, handle, dmclk_ioctl_cmd_get_tolerance, &tolerance);
```

##### dmclk_ioctl_cmd_get_oscillator_frequency

Gets the oscillator frequency.

```c
dmclk_frequency_t osc_freq;
int ret = dmclk_dmdrvi_ioctl(ctx, handle, dmclk_ioctl_cmd_get_oscillator_frequency, &osc_freq);
```

##### dmclk_ioctl_cmd_get_target_frequency

Gets the target frequency.

```c
dmclk_frequency_t target;
int ret = dmclk_dmdrvi_ioctl(ctx, handle, dmclk_ioctl_cmd_get_target_frequency, &target);
```

#### Set Commands

All set commands automatically trigger a clock reconfiguration after updating the parameter.

##### dmclk_ioctl_cmd_set_source

Sets the clock source.

```c
dmclk_source_t source = dmclk_source_external;
int ret = dmclk_dmdrvi_ioctl(ctx, handle, dmclk_ioctl_cmd_set_source, &source);
```

##### dmclk_ioctl_cmd_set_tolerance

Sets the frequency tolerance.

```c
dmclk_frequency_t tolerance = 1000; // ±1 kHz
int ret = dmclk_dmdrvi_ioctl(ctx, handle, dmclk_ioctl_cmd_set_tolerance, &tolerance);
```

##### dmclk_ioctl_cmd_set_oscillator_frequency

Sets the oscillator frequency.

```c
dmclk_frequency_t osc_freq = 8000000; // 8 MHz
int ret = dmclk_dmdrvi_ioctl(ctx, handle, dmclk_ioctl_cmd_set_oscillator_frequency, &osc_freq);
```

##### dmclk_ioctl_cmd_set_target_frequency

Sets the target frequency.

```c
dmclk_frequency_t target = 100000000; // 100 MHz
int ret = dmclk_dmdrvi_ioctl(ctx, handle, dmclk_ioctl_cmd_set_target_frequency, &target);
```

##### dmclk_ioctl_cmd_reconfigure

Reconfigures the clock with current settings without changing any parameters.

```c
int ret = dmclk_dmdrvi_ioctl(ctx, handle, dmclk_ioctl_cmd_reconfigure, NULL);
```

### dmclk_dmdrvi_flush

```c
int dmclk_dmdrvi_flush(dmdrvi_context_t context, void* handle);
```

Flushes device buffers - no operation for clock device.

**Returns:** 0

### dmclk_dmdrvi_stat

```c
int dmclk_dmdrvi_stat(dmdrvi_context_t context, void* handle, dmdrvi_stat_t* stat);
```

Gets device statistics.

**Parameters:**
- `context`: Device context
- `handle`: Device handle
- `stat`: Pointer to stat structure to fill

**Returns:**
- 0 on success
- Negative error code on failure

**Stat Fields:**
- `size`: Size of the read data string
- `mode`: File permissions (0444 - read-only)

## Port Layer API

The port layer implements hardware-specific clock configuration. These functions are called internally by the core module.

### dmclk_port_configure_internal

```c
int dmclk_port_configure_internal(dmclk_frequency_t target_freq, dmclk_frequency_t tolerance);
```

Configures the system clock using the internal oscillator.

### dmclk_port_configure_external

```c
int dmclk_port_configure_external(dmclk_frequency_t target_freq, dmclk_frequency_t tolerance, dmclk_frequency_t oscillator_freq);
```

Configures the system clock using an external crystal or oscillator.

### dmclk_port_configure_hibernatation

```c
int dmclk_port_configure_hibernatation(dmclk_frequency_t target_freq, dmclk_frequency_t tolerance, dmclk_frequency_t oscillator_freq);
```

Configures the system clock using the hibernation clock source.

**Note:** Function name contains a typo (should be "hibernation") but is kept for API compatibility.

### dmclk_port_delay_us

```c
void dmclk_port_delay_us(dmclk_time_us_t time_us);
```

Delays execution for the specified number of microseconds.

### dmclk_port_get_current_frequency

```c
dmclk_frequency_t dmclk_port_get_current_frequency(void);
```

Returns the current system clock frequency in Hz.

## Error Codes

The module uses standard errno error codes:

| Code | Description |
|------|-------------|
| 0 | Success |
| -EINVAL | Invalid parameter or configuration |
| -ENOMEM | Memory allocation failure |

Error messages are logged using the DMOD logging system (DMOD_LOG_ERROR, DMOD_LOG_INFO).

## Thread Safety

The DMCLK module is not inherently thread-safe. If multiple threads need to access the same device context, external synchronization must be provided by the application.

## Example: Complete Usage

```c
#include "dmclk.h"
#include "dmdrvi.h"
#include "dmini.h"
#include <stdio.h>

int main(void)
{
    // Load configuration file
    dmini_context_t config = dmini_load("config.ini");
    if (config == NULL) {
        printf("Failed to load config\n");
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

    // Open device
    void* handle = dmclk_dmdrvi_open(ctx, DMDRVI_O_RDONLY);
    if (handle == NULL) {
        printf("Failed to open device\n");
        dmclk_dmdrvi_free(ctx);
        dmini_free(config);
        return -1;
    }

    // Read current configuration
    char buffer[256];
    dmclk_dmdrvi_read(ctx, handle, buffer, sizeof(buffer));
    printf("Current config: %s\n", buffer);

    // Get actual frequency
    dmclk_frequency_t freq;
    dmclk_dmdrvi_ioctl(ctx, handle, dmclk_ioctl_cmd_get_frequency, &freq);
    printf("Actual frequency: %lu Hz\n", freq);

    // Change to 100 MHz
    dmclk_frequency_t new_freq = 100000000;
    if (dmclk_dmdrvi_ioctl(ctx, handle, dmclk_ioctl_cmd_set_target_frequency, &new_freq) == 0) {
        dmclk_dmdrvi_ioctl(ctx, handle, dmclk_ioctl_cmd_get_frequency, &freq);
        printf("New frequency: %lu Hz\n", freq);
    }

    // Cleanup
    dmclk_dmdrvi_close(ctx, handle);
    dmclk_dmdrvi_free(ctx);
    dmini_free(config);

    return 0;
}
```
