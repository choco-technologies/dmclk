# Port Implementation Guide

## Overview

The port layer provides hardware-specific implementations of clock configuration for different microcontroller families. This guide explains how to add support for new hardware platforms.

## Port Layer Architecture

The DMCLK module separates platform-independent logic (core) from hardware-specific code (port):

```
dmclk (core)
    ↓
dmclk_port API (abstraction)
    ↓
Platform-specific implementation
```

## Required Port Functions

Every port implementation must provide these five functions:

### 1. dmclk_port_configure_internal

```c
int dmclk_port_configure_internal(dmclk_frequency_t target_freq, dmclk_frequency_t tolerance);
```

Configures the system clock using the internal oscillator.

**Parameters:**
- `target_freq`: Desired frequency in Hz
- `tolerance`: Acceptable deviation in Hz

**Returns:**
- 0 on success
- Negative error code on failure

### 2. dmclk_port_configure_external

```c
int dmclk_port_configure_external(dmclk_frequency_t target_freq, dmclk_frequency_t tolerance, dmclk_frequency_t oscillator_freq);
```

Configures the system clock using an external crystal or oscillator.

**Parameters:**
- `target_freq`: Desired frequency in Hz
- `tolerance`: Acceptable deviation in Hz
- `oscillator_freq`: External oscillator frequency in Hz

**Returns:**
- 0 on success
- Negative error code on failure

### 3. dmclk_port_configure_hibernatation

```c
int dmclk_port_configure_hibernatation(dmclk_frequency_t target_freq, dmclk_frequency_t tolerance, dmclk_frequency_t oscillator_freq);
```

Configures the system clock using the hibernation/low-power clock source.

**Note:** Function name contains a typo but is kept for API compatibility.

**Parameters:**
- `target_freq`: Desired frequency in Hz
- `tolerance`: Acceptable deviation in Hz
- `oscillator_freq`: Hibernation oscillator frequency in Hz

**Returns:**
- 0 on success
- Negative error code on failure

### 4. dmclk_port_delay_us

```c
void dmclk_port_delay_us(dmclk_time_us_t time_us);
```

Delays execution for specified microseconds.

**Parameters:**
- `time_us`: Time to delay in microseconds

### 5. dmclk_port_get_current_frequency

```c
dmclk_frequency_t dmclk_port_get_current_frequency(void);
```

Returns the current system clock frequency.

**Returns:** Current frequency in Hz

## Implementation Approaches

### Approach 1: Simple Direct Implementation

For unique microcontrollers, implement all functions directly:

```
port/
└── my_mcu/
    └── port.c  (implements all 5 functions)
```

### Approach 2: Shared Common Code

For MCU families with similarities, extract common code:

```
port/
├── my_mcu_common/
│   ├── my_mcu_common.h
│   └── my_mcu_common.c  (shared functions)
├── my_mcu_series1/
│   └── port.c  (series-specific, calls common functions)
└── my_mcu_series2/
    └── port.c  (series-specific, calls common functions)
```

## Step-by-Step: Adding a New STM32 Family

### Step 1: Create Header with Register Definitions

Create `include/port/stm32xx_regs.h`:

```c
#ifndef STM32XX_REGS_H
#define STM32XX_REGS_H

// Base addresses
#define RCC_BASE        0x40023800
#define FLASH_BASE      0x40023C00

// Clock limits
#define MAX_SYSCLK      168000000  // Maximum system clock
#define MAX_APB1        42000000   // Maximum APB1 bus clock
#define MAX_APB2        84000000   // Maximum APB2 bus clock

// Flash latency table
// [voltage_range][frequency_range] = wait_states
static const uint8_t flash_latency_table[3][8] = {
    // ... latency values ...
};

#endif
```

### Step 2: Create Port Implementation

Create `src/port/stm32xx/port.c`:

```c
#define DMOD_ENABLE_REGISTRATION ON
#include "dmclk_port.h"
#include "stm32_common.h"
#include "stm32xx_regs.h"

// Define clock limits for this family
const stm32_clock_limits_t stm32_limits = {
    .max_sysclk = MAX_SYSCLK,
    .max_apb1 = MAX_APB1,
    .max_apb2 = MAX_APB2,
    .flash_latency_table = (const uint8_t*)flash_latency_table,
};

// Use common STM32 implementation
dmod_dmclk_port_api_declaration(1.0, int, _configure_internal, (dmclk_frequency_t target_freq, dmclk_frequency_t tolerance))
{
    return stm32_configure_internal(target_freq, tolerance);
}

dmod_dmclk_port_api_declaration(1.0, int, _configure_external, (dmclk_frequency_t target_freq, dmclk_frequency_t tolerance, dmclk_frequency_t oscillator_freq))
{
    return stm32_configure_external(target_freq, tolerance, oscillator_freq);
}

// ... remaining functions ...
```

### Step 3: Update CMakeLists.txt

The build system automatically includes STM32 common code. Just build with:

```bash
cmake -DDMCLK_MCU_SERIES=stm32xx ..
```

## Step-by-Step: Adding a Non-STM32 MCU

### Step 1: Analyze Hardware

Understand your MCU's clock architecture:
- Clock sources available (internal RC, external crystal, PLL, etc.)
- PLL capabilities and limitations
- Bus/peripheral clock dividers
- Maximum frequencies
- Flash wait state requirements

### Step 2: Create Port Directory

```bash
mkdir -p src/port/my_mcu
```

### Step 3: Implement Port Functions

Create `src/port/my_mcu/port.c`:

```c
#define DMOD_ENABLE_REGISTRATION ON
#include "dmclk_port.h"
#include <stdint.h>

// Hardware register definitions
#define CLOCK_CTRL_REG  (*(volatile uint32_t*)0x40000000)
#define PLL_CONFIG_REG  (*(volatile uint32_t*)0x40000004)
// ... more registers ...

// Internal oscillator configuration
dmod_dmclk_port_api_declaration(1.0, int, _configure_internal, 
    (dmclk_frequency_t target_freq, dmclk_frequency_t tolerance))
{
    // 1. Calculate dividers/multipliers
    // 2. Validate against tolerance
    // 3. Configure hardware registers
    // 4. Wait for clock stability
    // 5. Update flash wait states if needed
    // 6. Switch to new clock source
    
    return 0;  // Success
}

// External oscillator configuration
dmod_dmclk_port_api_declaration(1.0, int, _configure_external,
    (dmclk_frequency_t target_freq, dmclk_frequency_t tolerance, dmclk_frequency_t oscillator_freq))
{
    // Similar to internal, but use oscillator_freq as PLL input
    return 0;
}

// Hibernation/low-power clock configuration
dmod_dmclk_port_api_declaration(1.0, int, _configure_hibernatation,
    (dmclk_frequency_t target_freq, dmclk_frequency_t tolerance, dmclk_frequency_t oscillator_freq))
{
    // Configure low-power clock source
    return 0;
}

// Microsecond delay
dmod_dmclk_port_api_declaration(1.0, void, _delay_us,
    (dmclk_time_us_t time_us))
{
    // Implement delay using timer or busy loop
    // Account for current clock frequency
}

// Get current frequency
dmod_dmclk_port_api_declaration(1.0, dmclk_frequency_t, _get_current_frequency,
    (void))
{
    // Read hardware registers
    // Calculate actual frequency based on configuration
    return frequency;
}
```

### Step 4: Update Build System

Modify `src/port/CMakeLists.txt` to include your MCU:

```cmake
# Detect MCU vendor from series name
if(DMCLK_MCU_SERIES MATCHES "^my_mcu")
    set(MCU_VENDOR "my_vendor")
    set(PORT_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/${DMCLK_MCU_SERIES}/port.c)
endif()
```

### Step 5: Test

Build and test your implementation:

```bash
cmake -DDMCLK_MCU_SERIES=my_mcu ..
cmake --build .
```

## Implementation Guidelines

### Clock Configuration Algorithm

1. **Disable interrupts** (if necessary for atomic operation)
2. **Enable clock source** (internal RC, external crystal, etc.)
3. **Wait for stability** (check ready flags)
4. **Configure PLL** (if using PLL):
   - Calculate input divider (PLL input frequency)
   - Calculate multiplier (VCO frequency)
   - Calculate output dividers (system clock, peripheral clocks)
   - Validate against hardware limits
5. **Configure flash wait states** (before increasing frequency)
6. **Configure bus dividers** (to keep peripherals within limits)
7. **Switch system clock source**
8. **Wait for switch confirmation**
9. **Update bus dividers** (if reducing frequency)
10. **Re-enable interrupts**

### Error Handling

Return appropriate error codes:

```c
#include <errno.h>

// Invalid parameters
return -EINVAL;

// Hardware error (oscillator won't start, etc.)
return -EIO;

// Frequency not achievable within tolerance
return -ERANGE;
```

### Validation

Always validate that achieved frequency is within tolerance:

```c
dmclk_frequency_t actual_freq = calculate_actual_frequency(pll_config);
dmclk_frequency_t deviation = (actual_freq > target_freq) ? 
    (actual_freq - target_freq) : (target_freq - actual_freq);

if (deviation > tolerance) {
    return -ERANGE;
}
```

### Flash Wait States

Critical for reliability:

```c
// Increase wait states BEFORE increasing frequency
if (new_freq > current_freq) {
    configure_flash_wait_states(new_freq);
}

// Switch to new frequency
switch_clock_source();

// Decrease wait states AFTER decreasing frequency
if (new_freq < current_freq) {
    configure_flash_wait_states(new_freq);
}
```

## STM32 Common Implementation

The existing STM32 implementation provides reusable functions:

### stm32_common.h Functions

```c
// Calculate PLL parameters
int stm32_calculate_pll(uint32_t target_freq, uint32_t input_freq,
                        stm32_pll_config_t* config);

// Configure flash wait states
void stm32_set_flash_latency(uint32_t freq_hz);

// Configure APB dividers
void stm32_configure_bus_dividers(uint32_t sysclk);

// Apply PLL configuration
int stm32_apply_pll_config(const stm32_pll_config_t* config);
```

## Testing Your Port

### Test Plan

1. **Basic functionality:**
   - Configure internal clock
   - Configure external clock
   - Verify actual frequency

2. **Boundary conditions:**
   - Minimum frequency
   - Maximum frequency
   - Invalid parameters

3. **Runtime reconfiguration:**
   - Change frequency up
   - Change frequency down
   - Change clock source

4. **Stress testing:**
   - Rapid reconfigurations
   - Operation under load

### Example Test Code

```c
void test_clock_configuration(void)
{
    dmini_context_t config = dmini_create();
    dmdrvi_dev_num_t dev_num = {0};
    
    // Test 1: Internal clock
    dmini_set_string(config, "dmclk", "source", "internal");
    dmini_set_int(config, "dmclk", "target_frequency", 16000000);
    dmini_set_int(config, "dmclk", "tolerance", 1000);
    
    dmdrvi_context_t ctx = dmclk_dmdrvi_create(config, &dev_num);
    assert(ctx != NULL);
    
    // Test 2: External clock
    dmini_set_string(config, "dmclk", "source", "external");
    dmini_set_int(config, "dmclk", "target_frequency", 84000000);
    dmini_set_int(config, "dmclk", "oscillator_frequency", 8000000);
    
    // ... more tests ...
}
```

## Common Pitfalls

1. **Not waiting for clock stability:** Always check ready flags
2. **Wrong flash wait states:** Can cause crashes or corruption
3. **Bus clocks exceeding limits:** Can damage peripherals
4. **Integer overflow:** Use 64-bit arithmetic for frequency calculations
5. **Not handling PLL lock failure:** Always timeout and return error
6. **Forgetting to enable clock source:** Must enable before use

## Resources

### STM32 Reference

- STM32F4 Reference Manual: Clock configuration (RCC chapter)
- STM32F7 Reference Manual: Clock configuration (RCC chapter)
- Application Notes: Clock configuration examples

### Tools

- STM32CubeMX: Visual clock configuration tool (reference only, not used in runtime)
- Clock tree diagrams: Available in reference manuals

## See Also

- [API Reference](api-reference.md) - Port API function details
- [Configuration Guide](configuration.md) - Clock configuration examples
- [Main Documentation](dmclk.md) - Module overview
