# SAI kernel clocks

`dmclk_sai_acquire(domain, target_hz, tolerance_hz, &actual_hz)` reserves
SAI1 or SAI2 from PLLI2S Q/DIVQ. `dmclk_sai_release(domain)` releases one
successful reservation. SYSCLK, the shared PLLM, PLLSAI and LCD clock fields
are preserved. Stop and gate the SAI peripheral before its final release.

The STM32 implementation lives in `src/port/stm32_common`. Family ports supply
addresses, clock limits and capabilities; they contain no duplicate clock
configuration logic. STM32F7 advertises the PLLI2S Q/DIVQ SAI path. The current
STM32F4 port does not advertise it and returns `-ENOTSUP`. These are the only
two production ports in this repository. Moving code into the common layer
does not imply that every STM32 family implements this RCC layout.

## Ownership and errors

Operations run in thread context with a working `Dmod_GetUptime()` millisecond
clock. IRQ masking only protects the operation lock; PLL waits leave interrupts
enabled and time out after 100 ms per transition. A concurrent operation returns
`-EBUSY` immediately, including when the owning thread is preempted. The caller
may retry after that operation finishes.

Compatible reservations share the programmed frequency, including across
SAI1/SAI2. The following conditions also return `-EBUSY`:

- A conflicting frequency, externally enabled PLLI2S or externally owned SAI.
- The final release while the consumer's SAI peripheral clock is enabled.
- HSI, HSE or hibernation configuration while a reservation exists.
- Port deinitialization while a reservation exists. Normal disable/unload
  must release every reservation first; forced cleanup of another driver's
  active hardware is not provided.
- Main-clock configuration or deinitialization while failed-acquire recovery
  is pending.

Invalid arguments return `-EINVAL`; an unachievable tolerance returns `-ERANGE`.
A source that is not ready, or an HSE frequency unknown to dmclk, returns
`-ENODATA`. HSE configured by a bootloader is not sufficient to discover its
physical frequency: configure it through dmclk before acquiring SAI. The HSE
cache is published only after successful configuration; common configuration
helpers receive the oscillator frequency explicitly and do not read the cache.
Each SAI domain supports 255 simultaneous reservations; the next acquire returns
`-EOVERFLOW`. Output arguments remain unchanged on failure.

## Timeout recovery

A lock or stop timeout returns `-ETIMEDOUT`. On a normal last release, the
previous mux, divider and PLL fields are restored. If the PLL does not stop
during release, ownership is retained; stop/gate the consumer and retry release.

If acquisition fails to lock, it attempts to stop the PLL and restore the
saved fields. A late lock can also make that stop time out. In this case no
reservation is granted and the mux is not switched, but the original register
snapshot is retained. A later acquire first retries stop/restoration; it cannot
replace the saved snapshot until recovery succeeds. Main-clock configuration
and deinitialization remain blocked during recovery. A permanently stuck PLL
requires hardware recovery/reset.

Do not write the saved PLL parameters while the ready flag remains asserted.
[RM0385, RCC PLLI2S configuration register](https://www.st.com/resource/en/reference_manual/dm00124865-stm32f75xxx-andstm32f74xxx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
requires PLLI2S to be disabled when writing its multiplication/division factors.

## Frequency selection

For STM32F746G-DISCO's 25 MHz HSE, request `sample_rate * 256` with an
absolute tolerance such as `target_hz / 2000` (500 ppm). The solver uses the
currently programmed shared PLLM; it does not change it to improve audio.
`actual_hz` is rounded from register readback. Tolerance and sharing decisions
use the unrounded rational frequency.

`dmclk_port_get_domain_frequency(dmclk_domain_sai2)` reads this PLLI2S clock
path. It returns zero if not ready, the source frequency is unknown, or another
mux source is selected. Outside drivers must not bypass the reservation API
to change PLLI2S or the shared PLL source/M divider.

## Version and validation

Module version 1.2 adds the SAI entry points to released version 1.1.2. The old
CMake default 0.1 was stale. API version 1.0 is the per-symbol ABI version:
existing signatures are unchanged and the two new symbols are implemented by
both ports (STM32F4 returns `-ENOTSUP`). Consumers of the new symbols require
module version 1.2 or newer.

Host tests call the port API against simulated STM32 register memory. The
production CMake source list builds the same shared implementation into a
native target; tests neither include implementation `.c` files nor access its
private reservation state. The fixture supplies MMIO, uptime and critical
sections. Coverage includes rates, sharing, invalid inputs, unknown/not-ready
sources, overflow, concurrent calls, main-clock/hibernation/deinit guards and
both timeout recovery paths.

```sh
cmake -S tests/ports/stm32 -B build-stm32-tests
cmake --build build-stm32-tests
ctest --test-dir build-stm32-tests --output-on-failure
```

On initialized STM32F746G-DISCO, run `dmclk_sai_test`. The board-specific test
is in `tests/ports/stm32f7`. It checks 44.1/48 kHz, sharing, conflicts, readback
and register restoration. Main-clock negative tests run only on the host
fixture so a broken guard cannot reprogram SYSCLK on the board. It does not
measure the physical MCLK pin.
