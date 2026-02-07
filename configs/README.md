# DMCLK Configuration Files

This directory contains default clock configuration files for various STM32 development boards and microcontrollers. These configuration files can be copied to your project during module installation using the `dmf-get` tool with the `--config-dir` option.

## Directory Structure

- **`board/`** - Configuration files for specific development boards
- **`mcu/`** - Configuration files for specific microcontrollers

## Usage

When installing the dmclk module, you can specify a configuration file in your `.dmd` dependencies file:

```dmd
# Install dmclk with configuration for STM32F746G-DISCO board
dmclk@1.0 board/stm32f746g-disco.ini

# Install dmclk with configuration for STM32F407VG MCU
dmclk@1.0 mcu/stm32f407vg.ini
```

Then install with:

```bash
dmf-get -d project-deps.dmd --config-dir ./config
```

This will copy the specified configuration file to your project's config directory.

## Board Configurations

Board configurations are optimized for specific development boards with known external oscillator frequencies and target maximum performance:

| Board | MCU | Oscillator | Max Frequency | File |
|-------|-----|------------|---------------|------|
| STM32F4DISCOVERY | STM32F407VG | 8 MHz | 168 MHz | `board/stm32f4-discovery.ini` |
| STM32F429I-DISCOVERY | STM32F429ZI | 8 MHz | 180 MHz | `board/stm32f429i-discovery.ini` |
| NUCLEO-F401RE | STM32F401RE | 8 MHz | 84 MHz | `board/nucleo-f401re.ini` |
| NUCLEO-F411RE | STM32F411RE | 8 MHz | 100 MHz | `board/nucleo-f411re.ini` |
| NUCLEO-F446RE | STM32F446RE | 8 MHz | 180 MHz | `board/nucleo-f446re.ini` |
| STM32F746G-DISCO | STM32F746NG | 25 MHz | 216 MHz | `board/stm32f746g-disco.ini` |
| NUCLEO-F767ZI | STM32F767ZI | 8 MHz | 216 MHz | `board/nucleo-f767zi.ini` |
| STM32F769I-DISCOVERY | STM32F769NI | 25 MHz | 216 MHz | `board/stm32f769i-discovery.ini` |

## MCU Configurations

MCU configurations provide default settings for specific microcontrollers, assuming a common 8 MHz external oscillator:

### STM32F4 Series

| MCU | Max Frequency | File |
|-----|---------------|------|
| STM32F401RE | 84 MHz | `mcu/stm32f401re.ini` |
| STM32F405RG | 168 MHz | `mcu/stm32f405rg.ini` |
| STM32F407VG | 168 MHz | `mcu/stm32f407vg.ini` |
| STM32F411RE | 100 MHz | `mcu/stm32f411re.ini` |
| STM32F429ZI | 180 MHz | `mcu/stm32f429zi.ini` |
| STM32F439ZI | 180 MHz | `mcu/stm32f439zi.ini` |
| STM32F446RE | 180 MHz | `mcu/stm32f446re.ini` |
| STM32F469NI | 180 MHz | `mcu/stm32f469ni.ini` |

### STM32F7 Series

| MCU | Max Frequency | File |
|-----|---------------|------|
| STM32F722RE | 216 MHz | `mcu/stm32f722re.ini` |
| STM32F746ZG | 216 MHz | `mcu/stm32f746zg.ini` |
| STM32F767ZI | 216 MHz | `mcu/stm32f767zi.ini` |
| STM32F769NI | 216 MHz | `mcu/stm32f769ni.ini` |

## Configuration Format

All configuration files use the INI format with the `[dmclk]` section:

```ini
[dmclk]
source=external              # Clock source: internal, external, or hibernation
target_frequency=168000000   # Target frequency in Hz
tolerance=1000               # Acceptable deviation in Hz
oscillator_frequency=8000000 # External oscillator frequency in Hz
```

### Parameters

- **source**: Clock source type
  - `internal` - Internal RC oscillator (HSI)
  - `external` - External crystal or oscillator (HSE)
  - `hibernation` - Low-power hibernation clock (LSI/LSE)

- **target_frequency**: Desired system clock frequency in Hz

- **tolerance**: Maximum acceptable deviation from target frequency in Hz

- **oscillator_frequency**: External oscillator/crystal frequency in Hz (required for external and hibernation sources)

## Customization

You can modify these configuration files for your specific hardware:

1. Copy the configuration file to your project
2. Adjust the `oscillator_frequency` if you use a different crystal
3. Modify the `target_frequency` if you need a different clock speed
4. Adjust the `tolerance` based on your timing requirements

## See Also

- [DMCLK Configuration Guide](../docs/configuration.md) - Detailed configuration documentation
- [DMR File Format](https://github.com/choco-technologies/dmod/blob/develop/docs/dmr-file-format.md) - Resource file format
- [DMD File Format](https://github.com/choco-technologies/dmod/blob/develop/docs/dmd-file-format.md) - Dependencies file format
