# SAI kernel clocks (STM32F7)

`dmclk_sai_acquire(domain, target_hz, tolerance_hz, &actual_hz)` reserves
SAI1 or SAI2 from PLLI2S Q/DIVQ. It preserves SYSCLK, PLLM, PLLSAI and LCD
clock fields. `dmclk_sai_release(domain)` releases one successful reservation.
Operations run in thread context. IRQ masking only protects the operation
lock; PLL lock waits run with interrupts enabled and are bounded.

A compatible reservation shares the programmed frequency, including across
SAI1/SAI2. A conflicting frequency, externally enabled PLLI2S, or SAI peripheral
owned outside this API returns `-EBUSY`. Stop and gate the SAI peripheral before
its final release. Last release restores prior mux, divider and PLL settings.
Unsupported ports (STM32F4) return `-ENOTSUP`. Invalid arguments return `-EINVAL`,
unachievable tolerance `-ERANGE`, absent source `-ENODATA`, and PLL timeout
`-ETIMEDOUT`. Output arguments remain unchanged on failure. Calls to configure
HSI/HSE through this port return `-EBUSY` while SAI clock reservations exist.

For the STM32F746G-DISCO 25 MHz HSE, request `sample_rate * 256` with an
absolute tolerance such as `target_hz / 2000` (500 ppm). The solver uses the
currently programmed shared PLLM; it does not change it to improve audio.
`actual_hz` is rounded from register readback; consumers should report the
actual sample rate. `dmclk_port_get_domain_frequency(dmclk_domain_sai2)` reads
this PLLI2S clock path; returns zero if not ready or a different mux source is
selected. Other source paths are intentionally outside this implementation.

Use PLLI2S for audio and PLLSAI for LCD. Outside drivers must not bypass the
reservation API to change PLLI2S or the shared PLL source/M divider.

Host tests: `cmake -S tests/sai-clock -B build-sai; cmake --build build-sai;
ctest --test-dir build-sai --output-on-failure`.
Hardware: run `dmclk_sai_test` on an initialized STM32F746G-DISCO. It checks
44.1/48 kHz families, sharing/conflict handling, main-clock protection and
register restoration. It does not measure the physical MCLK pin.
