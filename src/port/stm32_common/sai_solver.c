/* PLLI2S Q / DIVQ, STM32F746 RM0385. Never changes the shared PLLM. */
#include "sai_clock.h"
#include <errno.h>
#include <limits.h>

int stm32_sai_solve(uint32_t source, uint32_t m, uint32_t target,
                     uint32_t tolerance, stm32_sai_setting_t *setting)
{
    if (!setting || !target || m < 2 || m > 63 ||
        (uint64_t)source < (uint64_t)m * 1000000 ||
        (uint64_t)source > (uint64_t)m * 2000000)
        return -EINVAL;
    uint64_t best_error = UINT64_MAX, best_denominator = 1;
    stm32_sai_setting_t best = {0};
    for (uint32_t n = 50; n <= 432; n++)
    {
        uint64_t numerator = (uint64_t)source * n;
        if (numerator < (uint64_t)m * 100000000 ||
            numerator > (uint64_t)m * 432000000) continue;
        for (uint32_t q = 2; q <= 15; q++)
        {
            for (uint32_t divq = 1; divq <= 32; divq++)
            {
                uint64_t denominator = (uint64_t)m * q * divq;
                uint64_t wanted = (uint64_t)target * denominator;
                uint64_t error = numerator > wanted ? numerator - wanted : wanted - numerator;
                if (error > (uint64_t)tolerance * denominator) continue;
                if (error * best_denominator >= best_error * denominator && best.frequency) continue;
                best_error = error;
                best_denominator = denominator;
                best = (stm32_sai_setting_t){n, q, divq,
                    (uint32_t)((numerator + denominator / 2) / denominator)};
            }
        }
    }
    if (!best.frequency) return -ERANGE;
    *setting = best;
    return 0;
}
