# DMCLK ports

The production ports are `stm32f4` and `stm32f7`. Each `port.c` supplies its
register addresses, clock limits, flash latency table and optional capabilities
through `stm32_clock_config_t`, then delegates initialization/deinitialization
to the shared implementation.

```
stm32_common/
  CMakeLists.txt     production source list, also used by native port tests
  clock.c/.h        clock configuration, delays, domain queries and lifecycle
  stm32_common.c/.h main PLL arithmetic and register helpers
  sai_clock.c/.h    SAI reservations, conflicts and timeout recovery
  sai_solver.c      PLLI2S Q/DIVQ frequency selection
stm32f4/port.c      STM32F4 parameters; SAI path unsupported
stm32f7/port.c      STM32F7 parameters; Over-Drive and SAI capabilities
```

Select the port with the existing `DMCLK_MCU_SERIES` CMake option. Shared
functions use the `stm32_` prefix; family names remain on actual family
parameters. No family port duplicates the shared register logic.

A new STM32 port must verify its RCC layout and clock constraints before using
these helpers. The current SAI implementation describes the PLLI2S Q/DIVQ
path used by STM32F7; the capability is opt-in. A port without this path must
implement the public SAI entry points with `-ENOTSUP`.

Native tests in `tests/ports/stm32` link the production shared target sources
through their CMake owner and call the public port API. The register fixture
supplies simulated hardware and SAL operations. Board-specific verification
is under `tests/ports/stm32f7`. See [SAI documentation](../../docs/sai-clock.md)
for ownership, timing, errors and recovery.
