# DMCLK on the ESP32-S3

The port (`src/port/esp32s3`) reads the CPU clock back from the registers and brings the PLL up / moves the CPU between its
frequencies. Build it with `-DDMCLK_MCU_SERIES=esp32s3` (tools: `arch/xtensa/esp32s3`).

| API | ESP32-S3 |
|-----|----------|
| `get_current_frequency` | the CPU clock, read back from the registers: XTAL (40 MHz / divider), PLL (80, 160 or 240 MHz) or RC fast |
| `configure_internal` / `configure_external` | the PLL frequency closest to the target within the tolerance: 80, 160 or 240 MHz. Nothing is touched when the CPU is within the tolerance already. `oscillator_freq` of `configure_external` must be 40 MHz (or 0): the only crystal that the chip takes |
| `configure_hibernatation` | `-ENOTSUP` |
| `delay_us`, `delay` | busy-wait on `CCOUNT` |
| `get_domain_frequency` | 0: the peripherals of the ESP32-S3 take their clock from the APB and have their own gates (in their drivers); no named domains |
| `acquire_domain` | `-ENOTSUP` |

## What a change of the frequency does

The ROM leaves the CPU on the XTAL divided by 2 (20 MHz) and nothing else starts the PLL, so the first `configure_*` does what
ESP-IDF's second stage bootloader and startup code do:

1. The core voltage is set for the PLL frequency: the `dbias` values come from the calibration burnt in the eFuse (block version
   1.2 and later; the defaults of ESP-IDF otherwise), one value for 240 MHz and one for the lower frequencies.
2. The BBPLL is started at 480 MHz from the 40 MHz crystal (the analog I2C bus of the ROM) and its calibration is waited for.
3. The flash clock divider is set to keep the flash at 20 MHz when the APB goes from 20 to 80 MHz.
4. The CPU is moved to the PLL, and the ROM is told (`ets_update_cpu_frequency`) so `ets_delay_us` stays right.

Changes between 80, 160 and 240 MHz afterwards raise the voltage (and the powered LDO slaves) before going up and lower it after
going down. The whole sequence runs in a critical section.

## The tick of the RTOS

What counts the time from a fixed number of cycles does not follow a change of the frequency. The tick of FreeRTOS in dmod-boot
counts with `DMOSI_CPU_CLOCK_HZ` of the MCU (the same as the tick on the ARM ports), so it is right when dmclk configures the
CPU to that frequency (240 MHz by default; `configs/mcu/esp32s3.ini` and `configs/board/esp32s3-lcd28.ini` do). A CPU configured
to another frequency makes the tick run proportionally faster or slower.

## Limits

- **XTAL and RC fast are not targets**: the flash and the PSRAM run from the APB clock, which these sources would change.
- **The flash runs at 20 MHz** (the APB divided by 4). Faster flash needs the dummy cycles and the timing of the chip set for it.
- **No PLL at 320 MHz** is started; the 240 MHz needs the 480 MHz one. A CPU started on the 320 MHz PLL by somebody else can
  move between 80 and 160 MHz only (`-ENOTSUP` for 240).
- **Tested on the ESP32-S3FN16R8**: 80, 160 and 240 MHz from the start, and all the changes between them, with the CPU counter
  measured against the tick of the RTOS.
