#ifndef DMCLK_CLOCK_FIXTURE_H
#define DMCLK_CLOCK_FIXTURE_H
#include <stdbool.h>

void clock_fixture_init(bool sai_supported);
void clock_fixture_snapshot(void);
bool clock_fixture_unchanged(void);
void clock_fixture_fault(bool lock_timeout, bool stop_timeout);
void clock_fixture_source(bool ready, bool unknown_hse);
void clock_fixture_external_owner(bool pll, bool peripheral);
void clock_fixture_gate_sai(bool enabled);
void clock_fixture_concurrent_call(void);
int clock_fixture_concurrent_result(void);
int clock_fixture_deinit(void);
unsigned clock_fixture_elapsed_ms(void);
#endif
