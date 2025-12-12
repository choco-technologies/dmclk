# DMCLK Documentation Index

Welcome to DMCLK documentation. This module provides clock configuration and management for embedded systems.

## Available Documentation

### Getting Started

- **[Main Documentation](dmclk.md)** - Module overview, features, and quick start guide
- **[Configuration Guide](configuration.md)** - How to configure clock parameters
- **[Examples](examples.md)** - Practical code examples

### Reference

- **[API Reference](api-reference.md)** - Complete API documentation with all functions and types
- **[Port Implementation Guide](port-implementation.md)** - How to add support for new hardware platforms

## Quick Navigation

### By Topic

**Basic Usage:**
- [Quick Start](dmclk.md#quick-start)
- [Configuration Parameters](configuration.md#required-parameters)
- [Basic Example](examples.md#example-1-basic-clock-configuration)

**API Functions:**
- [Creating a Device](api-reference.md#dmclk_dmdrvi_create)
- [IOCTL Commands](api-reference.md#dmclk_dmdrvi_ioctl)
- [Reading Clock Info](api-reference.md#dmclk_dmdrvi_read)

**Platform Support:**
- [Supported Platforms](dmclk.md#platform-support)
- [Adding New Platform](port-implementation.md#step-by-step-adding-a-non-stm32-mcu)
- [STM32 Implementation](port-implementation.md#stm32-common-implementation)

**Configuration:**
- [Configuration Examples](configuration.md#configuration-examples)
- [Hardware-Specific Settings](configuration.md#hardware-specific-considerations)
- [Runtime Reconfiguration](configuration.md#runtime-reconfiguration)

### By Task

- **I want to configure my clock** → [Configuration Guide](configuration.md)
- **I need code examples** → [Examples](examples.md)
- **I want to understand the API** → [API Reference](api-reference.md)
- **I want to add a new MCU** → [Port Implementation Guide](port-implementation.md)
- **I want an overview** → [Main Documentation](dmclk.md)

## Module Information

- **Name:** dmclk
- **Description:** DMOD Clock Configuration Module
- **Author:** Patryk Kubiak
- **License:** MIT
- **Repository:** https://github.com/choco-technologies/dmclk

## See Also

- DMOD Framework: https://github.com/choco-technologies/dmod
- DMINI Module: https://github.com/choco-technologies/dmini
- DMDRVI Interface: https://github.com/choco-technologies/dmdrvi
