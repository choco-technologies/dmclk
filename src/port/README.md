# DMCLK Port Implementation

This directory contains hardware-specific implementations of the dmclk_port interface for various microcontroller families.

## Directory Structure

```
port/
├── stm32_common/          # Common code for all STM32 families
│   ├── stm32_common.h     # Shared declarations
│   └── stm32_common.c     # Shared implementation
├── stm32f0/               # STM32F0-specific implementation
│   └── port.c
├── stm32f1/               # STM32F1-specific implementation
│   └── port.c
├── stm32f2/               # STM32F2-specific implementation
│   └── port.c
├── stm32f3/               # STM32F3-specific implementation
│   └── port.c
├── stm32f4/               # STM32F4-specific implementation
│   └── port.c
├── stm32f7/               # STM32F7-specific implementation
│   └── port.c
├── stm32g0/               # STM32G0-specific implementation
│   └── port.c
├── stm32g4/               # STM32G4-specific implementation
│   └── port.c
├── stm32h7/               # STM32H7-specific implementation
│   └── port.c
├── stm32l0/               # STM32L0-specific implementation
│   └── port.c
├── stm32l1/               # STM32L1-specific implementation
│   └── port.c
├── stm32l4/               # STM32L4-specific implementation
│   └── port.c
├── stm32l5/               # STM32L5-specific implementation
│   └── port.c
├── stm32u5/               # STM32U5-specific implementation
│   └── port.c
├── stm32wb/               # STM32WB-specific implementation (Wireless)
│   └── port.c
├── stm32wl/               # STM32WL-specific implementation (Wireless)
│   └── port.c
└── CMakeLists.txt         # Build configuration
```

**Note:** Currently implemented families: STM32F4, STM32F7. Additional families listed above can be added following the same pattern.

## Adding Support for New Microcontroller Families

### For New STM32 Family (e.g., STM32H7, STM32L4)

1. **Create family-specific header** in `include/port/`:
   - Define memory addresses (RCC_BASE, FLASH_BASE)
   - Define clock limits (max frequencies, PLL ranges)
   - Define flash latency table
   
   Example: `include/port/stm32h7_regs.h`

2. **Create family directory** with `port.c`:
   - Include common headers
   - Define clock limits structure
   - Implement family-specific initialization if needed
   - Call common STM32 functions for clock configuration
   
   Example: `src/port/stm32h7/port.c`

3. **Update CMakeLists.txt**:
   - The build system automatically includes `stm32_common/` for any STM32 family
   - Just set `DMCLK_MCU_SERIES=stm32h7` when building

### For Non-STM32 Microcontrollers (e.g., NXP, TI, Microchip)

1. **Create vendor-specific common directory** (if multiple families):
   ```
   port/
   ├── nxp_common/         # Common code for NXP MCUs
   ├── imxrt1060/          # Specific NXP i.MX RT1060
   ```

2. **Implement port API functions**:
   - `dmclk_port_configure_internal()`
   - `dmclk_port_configure_external()`
   - `dmclk_port_configure_hibernatation()`
   - `dmclk_port_delay_us()`
   - `dmclk_port_get_current_frequency()`

3. **Update CMakeLists.txt**:
   - Add logic to detect vendor from `DMCLK_MCU_SERIES`
   - Include appropriate common sources
   - Example:
     ```cmake
     if(DMCLK_MCU_SERIES MATCHES "^imxrt")
         set(COMMON_SOURCES nxp_common/nxp_common.c)
     endif()
     ```

## Design Principles

1. **No External Dependencies**: All implementations use direct register access without HAL libraries
2. **Shared Code**: Common functionality is extracted to family-specific common directories
3. **Memory Map Separation**: Hardware addresses are kept in separate headers for easy porting
4. **Minimal Changes**: Family-specific code only contains what truly differs between variants

## Implementation Notes

### STM32 Implementation

The current STM32 implementation provides:
- **PLL Configuration**: Automatic calculation of PLL parameters (PLLM, PLLN, PLLP, PLLQ) based on target frequency
- **Clock Sources**: Support for HSI (internal), HSE (external), and LSI (low-power)
- **Flash Wait States**: Automatic configuration based on system clock frequency
- **Bus Prescalers**: Automatic APB1/APB2 prescaler calculation to stay within limits

### API Notes

Note: The API function name `dmclk_port_configure_hibernatation` contains a typo (should be "hibernation") but is kept as-is for API compatibility with the existing interface definition.
