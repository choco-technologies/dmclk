# DMCLK - DMOD Clock Configuration Module

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A DMOD (Dynamic Modular System) module for configuring and managing system clocks on embedded microcontrollers.

## Features

- **Multiple Clock Sources**: Internal RC oscillator, external crystal/oscillator, and hibernation clocks
- **Dynamic Configuration**: Runtime clock frequency adjustment
- **Hardware Abstraction**: Platform-independent API with hardware-specific implementations
- **DMDRVI Integration**: Full DMOD driver interface implementation
- **STM32 Support**: STM32F4 and STM32F7 families currently supported
- **Extensible**: Easy to add support for additional microcontroller families

## Quick Start

### Installation

Using `dmf-get` from the DMOD release package:

```bash
dmf-get install dmclk
```

Or install with a pre-configured setup for your board:

```bash
# Create a dependencies file (deps.dmd)
echo "dmclk@latest board/stm32f746g-disco.ini" > deps.dmd

# Install with configuration
dmf-get -d deps.dmd --config-dir ./config
```

The module includes ready-to-use configuration files for many popular STM32 boards and MCUs. See [`configs/README.md`](configs/README.md) for the complete list.

### Basic Usage

1. **Create a configuration file** (`config.ini`):

```ini
[dmclk]
source=external
target_frequency=84000000
tolerance=1000
oscillator_frequency=8000000
```

2. **Use in your code**:

```c
#include "dmclk.h"
#include "dmdrvi.h"
#include "dmini.h"

// Load configuration and create device
dmini_context_t config = dmini_load("config.ini");
dmdrvi_dev_num_t dev_num = {0};
dmdrvi_context_t clk_ctx = dmclk_dmdrvi_create(config, &dev_num);

// Open and use the clock device
void* handle = dmclk_dmdrvi_open(clk_ctx, DMDRVI_O_RDONLY);

// Get current frequency
dmclk_frequency_t freq;
dmclk_dmdrvi_ioctl(clk_ctx, handle, dmclk_ioctl_cmd_get_frequency, &freq);

// Cleanup
dmclk_dmdrvi_close(clk_ctx, handle);
dmclk_dmdrvi_free(clk_ctx);
dmini_free(config);
```

## Building

### Prerequisites

- CMake 3.18 or higher
- ARM GCC toolchain (for embedded targets)
- DMOD framework (automatically fetched)

### Build Commands

```bash
# Configure for STM32F4
cmake -DDMCLK_MCU_SERIES=stm32f4 -B build

# Configure for STM32F7
cmake -DDMCLK_MCU_SERIES=stm32f7 -B build

# Build
cmake --build build
```

## Documentation

Comprehensive documentation is available in the `docs/` directory:

- **[dmclk.md](docs/dmclk.md)** - Module overview and architecture
- **[api-reference.md](docs/api-reference.md)** - Complete API documentation
- **[configuration.md](docs/configuration.md)** - Configuration guide with examples
- **[port-implementation.md](docs/port-implementation.md)** - Guide for adding hardware support

View documentation using `dmf-man`:

```bash
dmf-man dmclk          # Main documentation
dmf-man dmclk api      # API reference
dmf-man dmclk config   # Configuration guide
dmf-man dmclk port     # Port implementation guide
```

## Supported Platforms

| Platform | Status | Notes |
|----------|--------|-------|
| STM32F4  | ✅ Supported | Full PLL configuration |
| STM32F7  | ✅ Supported | Full PLL configuration |
| Other STM32 | 🔧 In Progress | Easy to add using STM32 common code |
| Other MCUs | 📋 Planned | Contributions welcome |

## Pre-configured Boards and MCUs

The module includes ready-to-use configuration files for popular development boards and microcontrollers:

**Development Boards:**
- STM32F4DISCOVERY, STM32F429I-DISCOVERY
- NUCLEO-F401RE, NUCLEO-F411RE, NUCLEO-F446RE
- STM32F746G-DISCO, NUCLEO-F767ZI, STM32F769I-DISCOVERY

**Microcontrollers:**
- STM32F4 series: F401RE, F405RG, F407VG, F411RE, F429ZI, F439ZI, F446RE, F469NI
- STM32F7 series: F722RE, F746ZG, F767ZI, F769NI

For a complete list and usage instructions, see [`configs/README.md`](configs/README.md).

## Configuration Examples

### Internal Clock (16 MHz)

```ini
[dmclk]
source=internal
target_frequency=16000000
tolerance=160000
```

### External Crystal (84 MHz)

```ini
[dmclk]
source=external
target_frequency=84000000
tolerance=1000
oscillator_frequency=8000000
```

### Maximum Performance (STM32F4 - 168 MHz)

```ini
[dmclk]
source=external
target_frequency=168000000
tolerance=1000
oscillator_frequency=8000000
```

## Development

### Project Structure

```
dmclk/
├── configs/           # Pre-configured board and MCU configurations
│   ├── board/        # Board-specific configurations
│   └── mcu/          # MCU-specific configurations
├── docs/              # Documentation (markdown format)
├── examples/          # Example configurations
├── include/           # Public headers
│   ├── dmclk.h       # Main API
│   ├── dmclk_port.h  # Port layer API
│   └── port/         # Port-specific headers
├── src/
│   ├── dmclk.c       # Core implementation
│   └── port/         # Hardware-specific implementations
│       ├── stm32_common/  # Common STM32 code
│       ├── stm32f4/       # STM32F4 port
│       └── stm32f7/       # STM32F7 port
├── CMakeLists.txt    # Build configuration
└── manifest.dmm      # DMOD manifest
```

### Adding New Platform Support

See [Port Implementation Guide](docs/port-implementation.md) for detailed instructions on adding support for new microcontrollers.

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Submit a pull request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Authors

- Patryk Kubiak - Initial work

## Related Projects

- [DMOD](https://github.com/choco-technologies/dmod) - Dynamic Modular System framework
- [DMINI](https://github.com/choco-technologies/dmini) - INI configuration parser for DMOD
- [DMDRVI](https://github.com/choco-technologies/dmdrvi) - DMOD Driver Interface

## Support

For issues, questions, or contributions:

- Open an issue on GitHub
- Check the documentation in `docs/`
- Use `dmf-man dmclk` for command-line help
