# DMCLK on the ESP32-S3

The port (`src/port/esp32s3`) reads the CPU clock back from the registers (`SYSTEM_SYSCLK_CONF`, `SYSTEM_CPU_PER_CONF`) and moves
it between the frequencies of the PLL. Build it with `-DDMCLK_MCU_SERIES=esp32s3` (tools: `arch/xtensa/esp32s3`).

| API | ESP32-S3 |
|-----|----------|
| `get_current_frequency` | the CPU clock: XTAL (40 MHz / divider), PLL (80, 160 or 240 MHz) or RC fast, from the registers |
| `configure_internal` / `configure_external` | the PLL frequency closest to the target within the tolerance: 80, 160 or 240 MHz. Nothing is touched when the CPU is within the tolerance already. `oscillator_freq` of `configure_external` must be 40 MHz (or 0): the only crystal that the chip takes |
| `configure_hibernatation` | `-ENOTSUP` |
| `delay_us`, `delay` | busy-wait on `CCOUNT` |
| `get_domain_frequency` | 0: the peripherals of the ESP32-S3 take their clock from the APB and have their own gates (in their drivers); no named domains |
| `acquire_domain` | `-ENOTSUP` |

## Limits

- **The PLL is not brought up here.** `configure_*` returns `-ENOTSUP` when the CPU is not on the PLL. Note that the ROM leaves
  the CPU on the XTAL divided by 2 (20 MHz) and that nothing in dmod-boot's `esp32s3-lcd28` (the selector, dmboot) starts the PLL
  yet, so on that board this port can only report the frequency until the PLL is brought up (BBPLL through the analog I2C, the
  core voltage from the eFuse calibration, the flash clock afterwards - what ESP-IDF's second stage bootloader does).
- **No frequency above the one at the start of the module** (`-ERANGE`): the voltage of the core is set by the bootloader for it
  and is not raised here.
- **XTAL and RC fast are not targets**: the flash and the PSRAM run from the APB clock, which these sources would change.
- The ROM's idea of the CPU frequency (`ets_update_cpu_frequency`) is updated, so `ets_delay_us` stays right. What counts the
  time from a fixed number of cycles (the tick of FreeRTOS in dmod-boot) does not follow: after a change of the frequency the tick
  runs at a different rate.
