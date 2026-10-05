#ifndef DMCLK_TEST_DMOD_H
#define DMCLK_TEST_DMOD_H
#include <stdint.h>
#include <stddef.h>
typedef uint64_t Dmod_Timestamp_t;
void Dmod_EnterCritical(void);
void Dmod_ExitCritical(void);
Dmod_Timestamp_t Dmod_GetUptime(void);
#endif
