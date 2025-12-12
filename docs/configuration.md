# Configuration Guide

## Overview

DMCLK uses INI-style configuration files through the DMINI module. Configuration parameters control the clock source, target frequency, and tolerance.

## Configuration File Format

Configuration files use the INI format with a `[dmclk]` section:

```ini
[dmclk]
parameter=value
```

## Required Parameters

### source

**Type:** String  
**Values:** "internal", "external", "hibernation"  
**Description:** Specifies the clock source to use

- `internal`: Use internal RC oscillator (e.g., HSI on STM32)
- `external`: Use external crystal or oscillator (e.g., HSE on STM32)
- `hibernation`: Use low-power hibernation clock (e.g., LSI/LSE on STM32)

### target_frequency

**Type:** Integer  
**Unit:** Hz (Hertz)  
**Description:** The desired system clock frequency

The actual frequency achieved may differ slightly based on hardware limitations and PLL constraints. The tolerance parameter defines acceptable deviation.

### tolerance

**Type:** Integer  
**Unit:** Hz (Hertz)  
**Description:** Maximum acceptable deviation from target frequency

The configuration will fail if the hardware cannot achieve a frequency within `target_frequency ± tolerance`.

## Conditional Parameters

### oscillator_frequency

**Type:** Integer  
**Unit:** Hz (Hertz)  
**Required when:** source is "external" or "hibernation"  
**Description:** Frequency of the external oscillator or crystal

This parameter is mandatory when using external clock sources so the module can calculate appropriate PLL multipliers and dividers.

## Configuration Examples

### Example 1: Internal 16 MHz Clock

Simple configuration using the internal RC oscillator:

```ini
[dmclk]
source=internal
target_frequency=16000000
tolerance=160000
```

This configures a 16 MHz system clock with ±160 kHz tolerance (1% tolerance).

### Example 2: External Crystal at 84 MHz

High-performance configuration using an 8 MHz external crystal to achieve 84 MHz system clock:

```ini
[dmclk]
source=external
target_frequency=84000000
tolerance=1000
oscillator_frequency=8000000
```

### Example 3: External Crystal at 168 MHz (STM32F4/F7)

Maximum performance configuration for STM32F4/F7:

```ini
[dmclk]
source=external
target_frequency=168000000
tolerance=1000
oscillator_frequency=8000000
```

### Example 4: External 25 MHz Crystal

Configuration with a 25 MHz crystal:

```ini
[dmclk]
source=external
target_frequency=100000000
tolerance=1000
oscillator_frequency=25000000
```

### Example 5: Low-Power with 32.768 kHz Crystal

Low-power configuration using a watch crystal:

```ini
[dmclk]
source=hibernation
target_frequency=32768
tolerance=10
oscillator_frequency=32768
```

### Example 6: Tight Tolerance

Configuration requiring precise frequency:

```ini
[dmclk]
source=external
target_frequency=48000000
tolerance=100
oscillator_frequency=8000000
```

This requires the frequency to be within 48 MHz ± 100 Hz.

### Example 7: Loose Tolerance

Configuration with relaxed frequency requirements:

```ini
[dmclk]
source=internal
target_frequency=16000000
tolerance=1000000
```

This allows ±1 MHz deviation (±6.25% tolerance).

## Hardware-Specific Considerations

### STM32F4

**Maximum Frequency:** 168 MHz  
**Internal Oscillator:** 16 MHz (HSI)  
**External Oscillator Range:** 4-26 MHz  
**PLL Input Range:** 1-2 MHz  
**PLL Output Range:** 100-432 MHz  

**Recommended Configurations:**

```ini
# Maximum performance
[dmclk]
source=external
target_frequency=168000000
tolerance=1000
oscillator_frequency=8000000

# USB-compatible (48 MHz USB clock)
[dmclk]
source=external
target_frequency=168000000
tolerance=1000
oscillator_frequency=8000000
```

### STM32F7

**Maximum Frequency:** 216 MHz  
**Internal Oscillator:** 16 MHz (HSI)  
**External Oscillator Range:** 4-26 MHz  
**PLL Input Range:** 1-2 MHz  
**PLL Output Range:** 100-432 MHz  

**Recommended Configuration:**

```ini
# Maximum performance
[dmclk]
source=external
target_frequency=216000000
tolerance=1000
oscillator_frequency=8000000
```

## Loading Configuration

### From File

```c
#include "dmini.h"

dmini_context_t config = dmini_load("config.ini");
if (config == NULL) {
    // Handle error
}

// Use config with dmclk_dmdrvi_create
dmdrvi_context_t ctx = dmclk_dmdrvi_create(config, &dev_num);

// Free config when done
dmini_free(config);
```

### Programmatic Configuration

You can also create configuration programmatically using DMINI API:

```c
#include "dmini.h"

dmini_context_t config = dmini_create();
dmini_set_string(config, "dmclk", "source", "external");
dmini_set_int(config, "dmclk", "target_frequency", 84000000);
dmini_set_int(config, "dmclk", "tolerance", 1000);
dmini_set_int(config, "dmclk", "oscillator_frequency", 8000000);

// Use config
dmdrvi_context_t ctx = dmclk_dmdrvi_create(config, &dev_num);

// Free config
dmini_free(config);
```

## Runtime Reconfiguration

You can change clock parameters at runtime using IOCTL commands:

```c
// Change target frequency
dmclk_frequency_t new_freq = 100000000;
dmclk_dmdrvi_ioctl(ctx, handle, dmclk_ioctl_cmd_set_target_frequency, &new_freq);

// Change clock source
dmclk_source_t new_source = dmclk_source_internal;
dmclk_dmdrvi_ioctl(ctx, handle, dmclk_ioctl_cmd_set_source, &new_source);

// Reconfigure without changing parameters
dmclk_dmdrvi_ioctl(ctx, handle, dmclk_ioctl_cmd_reconfigure, NULL);
```

## Validation

The module validates configuration parameters:

1. **target_frequency > 0**: Must specify a frequency
2. **tolerance > 0**: Must specify tolerance
3. **source != unknown**: Must specify valid source
4. **oscillator_frequency > 0** when source is external/hibernation

If validation fails, `dmclk_dmdrvi_create` returns NULL and logs an error.

## Common Issues

### Configuration Fails to Load

**Problem:** `dmclk_dmdrvi_create` returns NULL

**Solutions:**
- Verify all required parameters are present
- Check parameter values are correct (positive integers, valid source string)
- Ensure oscillator_frequency is set for external/hibernation sources
- Check error logs using DMOD logging

### Frequency Not Achievable

**Problem:** Configuration loads but frequency is not as expected

**Solutions:**
- Increase tolerance value
- Choose a target frequency that's achievable with your oscillator
- For external sources, ensure oscillator_frequency matches your hardware
- Check hardware capabilities (maximum frequency, PLL ranges)

### Clock Unstable

**Problem:** System crashes or behaves erratically after configuration

**Solutions:**
- Verify oscillator_frequency matches actual crystal frequency
- Ensure voltage regulator can support target frequency
- Check flash wait states are correctly configured (handled automatically by port layer)
- Reduce target frequency

## Best Practices

1. **Start Conservative:** Begin with lower frequencies and increase gradually
2. **Use Appropriate Tolerance:** ±0.1% to ±1% is typical for most applications
3. **External for Precision:** Use external crystals for applications requiring precise timing (USB, CAN, etc.)
4. **Internal for Simplicity:** Use internal oscillator for applications that don't need precise timing
5. **Verify Configuration:** Always check actual frequency after configuration
6. **Test Thoroughly:** Test clock configuration under all operating conditions

## Configuration Templates

### USB Device

```ini
[dmclk]
source=external
target_frequency=48000000  # Or 96MHz, 168MHz with USB divider
tolerance=500
oscillator_frequency=8000000
```

### CAN Bus

```ini
[dmclk]
source=external
target_frequency=84000000
tolerance=1000
oscillator_frequency=8000000
```

### Low Power Application

```ini
[dmclk]
source=internal
target_frequency=4000000
tolerance=400000
```

### High Performance

```ini
[dmclk]
source=external
target_frequency=168000000  # Or 216MHz for STM32F7
tolerance=1000
oscillator_frequency=8000000
```

## See Also

- [API Reference](api-reference.md) - Complete API documentation
- [Port Implementation Guide](port-implementation.md) - Hardware-specific details
- [Main Documentation](dmclk.md) - Module overview
