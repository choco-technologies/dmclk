# DMCLK - DMOD Clock Configuration Module

## Overview

DMCLK is a DMOD (Dynamic Modular System) module that provides clock configuration and management functionality for embedded systems. It implements the DMDRVI (DMOD Driver Interface) to configure system clocks from various sources including internal oscillators, external crystals, and low-power hibernation clocks.

## Features

- **Multiple Clock Sources**: Support for internal RC oscillator, external crystal/oscillator, and hibernation clock sources
- **Dynamic Configuration**: Runtime clock frequency adjustment with tolerance control
- **Hardware Abstraction**: Platform-independent API with hardware-specific port implementations
- **DMDRVI Integration**: Full integration with DMOD driver interface for standardized device access
- **Read-Only Device**: Clock information accessible through standard read operations
- **IOCTL Interface**: Comprehensive control through IOCTL commands

## Architecture

The DMCLK module consists of two main components:

1. **Core Module** (`src/dmclk.c`): Implements the DMDRVI interface and manages clock configuration
2. **Port Layer** (`src/port/`): Hardware-specific implementations for different microcontroller families

### Supported Clock Sources

- `dmclk_source_internal`: Internal RC oscillator (HSI on STM32)
- `dmclk_source_external`: External crystal or oscillator (HSE on STM32)
- `dmclk_source_hibernation`: Low-power hibernation clock (LSI/LSE on STM32)

## Configuration

DMCLK uses DMINI configuration files to specify clock parameters. Configuration is read during device creation.

### Configuration Parameters

The module expects the following parameters in the `[dmclk]` section:

| Parameter | Type | Description | Required |
|-----------|------|-------------|----------|
| `source` | string | Clock source: "internal", "external", or "hibernation" | Yes |
| `target_frequency` | integer | Desired frequency in Hz | Yes |
| `tolerance` | integer | Acceptable frequency deviation in Hz | Yes |
| `oscillator_frequency` | integer | External oscillator frequency in Hz (for external/hibernation sources) | Conditional* |

*Required when using external or hibernation clock sources.

### Example Configuration

```ini
[dmclk]
source=external
target_frequency=84000000
tolerance=1000
oscillator_frequency=8000000
```

This example configures the system clock to 84 MHz using an 8 MHz external crystal with ±1 kHz tolerance.

## API Reference

### Device Operations

#### Creating a Device Context

```c
dmdrvi_context_t dmclk_dmdrvi_create(dmini_context_t config, const dmdrvi_dev_num_t* dev_num);
```

Creates a new DMCLK device context and applies the initial configuration.

**Parameters:**
- `config`: DMINI context containing clock configuration parameters
- `dev_num`: Device number structure

**Returns:** Device context handle or NULL on failure

#### Opening a Device

```c
void* dmclk_dmdrvi_open(dmdrvi_context_t context, int flags);
```

Opens a clock device for reading. Write-only access is not supported.

**Parameters:**
- `context`: Device context from `dmclk_dmdrvi_create`
- `flags`: Open flags (DMDRVI_O_WRONLY not supported)

**Returns:** Device handle or NULL on failure

#### Reading Clock Information

```c
size_t dmclk_dmdrvi_read(dmdrvi_context_t context, void* handle, void* buffer, size_t size);
```

Reads current clock configuration as a formatted string.

**Format:**
```
frequency=<value>;source=<string>;oscillator_frequency=<value>
```

**Example output:**
```
frequency=84000000;source=external;oscillator_frequency=8000000
```

### IOCTL Commands

DMCLK provides extensive control through IOCTL operations defined in `dmclk_ioctl_cmd_t`:

#### Query Operations

| Command | Argument Type | Description |
|---------|--------------|-------------|
| `dmclk_ioctl_cmd_get_frequency` | `dmclk_frequency_t*` | Get current actual frequency |
| `dmclk_ioctl_cmd_get_source` | `dmclk_source_t*` | Get current clock source |
| `dmclk_ioctl_cmd_get_tolerance` | `dmclk_frequency_t*` | Get frequency tolerance |
| `dmclk_ioctl_cmd_get_oscillator_frequency` | `dmclk_frequency_t*` | Get oscillator frequency |
| `dmclk_ioctl_cmd_get_target_frequency` | `dmclk_frequency_t*` | Get target frequency |

#### Configuration Operations

| Command | Argument Type | Description |
|---------|--------------|-------------|
| `dmclk_ioctl_cmd_set_source` | `dmclk_source_t*` | Set clock source |
| `dmclk_ioctl_cmd_set_tolerance` | `dmclk_frequency_t*` | Set frequency tolerance |
| `dmclk_ioctl_cmd_set_oscillator_frequency` | `dmclk_frequency_t*` | Set oscillator frequency |
| `dmclk_ioctl_cmd_set_target_frequency` | `dmclk_frequency_t*` | Set target frequency |
| `dmclk_ioctl_cmd_reconfigure` | NULL | Apply current configuration |

**Note:** Setting configuration parameters automatically triggers a reconfiguration.

### Example Usage

```c
#include "dmclk.h"
#include "dmdrvi.h"
#include "dmini.h"

// Load configuration
dmini_context_t config = dmini_load("config.ini");

// Create clock device
dmdrvi_dev_num_t dev_num = {0};
dmdrvi_context_t clk_ctx = dmclk_dmdrvi_create(config, &dev_num);

// Open device for reading
void* clk_handle = dmclk_dmdrvi_open(clk_ctx, DMDRVI_O_RDONLY);

// Read current clock info
char buffer[256];
dmclk_dmdrvi_read(clk_ctx, clk_handle, buffer, sizeof(buffer));

// Get actual frequency
dmclk_frequency_t actual_freq;
dmclk_dmdrvi_ioctl(clk_ctx, clk_handle, dmclk_ioctl_cmd_get_frequency, &actual_freq);

// Change target frequency
dmclk_frequency_t new_freq = 100000000; // 100 MHz
dmclk_dmdrvi_ioctl(clk_ctx, clk_handle, dmclk_ioctl_cmd_set_target_frequency, &new_freq);

// Close device
dmclk_dmdrvi_close(clk_ctx, clk_handle);
dmclk_dmdrvi_free(clk_ctx);
```

## Platform Support

DMCLK currently supports the following microcontroller families:

- **STM32F4**: Full support with PLL configuration
- **STM32F7**: Full support with PLL configuration

Additional STM32 families and other microcontrollers can be added through the port layer. See [Port Implementation Guide](port-implementation.md) for details.

## Error Handling

The module uses standard error codes from `<errno.h>`:

- `-EINVAL`: Invalid parameter or configuration
- Return value of 0: Success
- Return value < 0: Error code

Error messages are logged using DMOD_LOG_ERROR for debugging.

## Build Configuration

To build DMCLK for a specific platform, set the `DMCLK_MCU_SERIES` CMake variable:

```bash
cmake -DDMCLK_MCU_SERIES=stm32f4 ..
cmake --build .
```

## See Also

- [Port Implementation Guide](port-implementation.md) - How to add support for new hardware
- [Configuration Guide](configuration.md) - Detailed configuration examples
- [API Reference](api-reference.md) - Complete API documentation
