# Domain reservation hardware test

Run `domain_board_test` in the dmod-boot shell on STM32F746G-DISCO.
Build it together with `dmclk_port`, using the module version required by the
firmware's existing dmclk module. Stage the two `.dmf` files into the existing
dmod-boot `build/dmf` directory, remove the old `dmclk_port.dmfc` (which otherwise
shadows the local module), regenerate `modules.dmp`, then build/flash from that
dmod-boot build directory. The test calls only the public clock API; its direct
RCC accesses are read-only regression checks.

Validated on the attached STM32F746G-DISCO on 2026-10-08, with the existing
dmod-boot image and its PLLSAI-driven display still running:

```text
DOMAIN TEST: SYSCLK=216000000 CLK48=48000000 PLLSAI=0x54003000
SAI2 acquire target=12288000 actual=12285714 rc=0
SAI2 acquire target=11289600 actual=11289474 rc=0
DOMAIN BOARD TEST: PASS (0 failures)
```

Two consecutive runs passed, covering release and reacquisition. The test checks
reference sharing within SAI2 and between SAI1/SAI2, conflicting acquisitions,
rejected SYSCLK changes while reserved, unchanged SDIO/USB/RNG frequencies, and
restoration of the original RCC control, PLL and mux/divider registers. The
reported frequencies are calculated from hardware registers, not measured with
an external frequency counter; this test does not transmit audio.

The existing `pll_solver_test` also passed 8/8 on the board. Host tests passed
8/8 existing PLL regressions and 10/10 domain tests, including exhaustive solver
comparison, timeout rollback, active peripheral gates and borrowed PLLs.
