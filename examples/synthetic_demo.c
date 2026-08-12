#include <stdint.h>
#include <stdio.h>
#include "sigq15.h"
static const int16_t sine16[16] = {
    0,12539,23170,30274,32767,30274,23170,12539,
    0,-12539,-23170,-30274,-32768,-30274,-23170,-12539
};
int main(void)
{
    int16_t samples[256];
    for (size_t i = 0; i < 256; ++i)
        samples[i] = (int16_t)(((int32_t)sine16[i & 15U] * 16384) >> 15);
    sigq15_stats_result_t stats;
    sigq15_frequency_result_t frequency;
    sigq15_goertzel_result_t tone;
    sigq15_diag_t diag = {0};
    if (sigq15_stats(samples, 256, &stats) != SIGQ15_OK) return 1;
    if (sigq15_frequency_zero_cross(samples, 256, 1600, 1000,
                                    &frequency) != SIGQ15_OK) return 2;
    if (sigq15_goertzel(samples, 256, 1600, 100, &tone, &diag) != SIGQ15_OK)
        return 3;
    printf("RMS=%d/32768, f=%lu mHz, amplitude=%d/32768\n",
           stats.rms_q15, (unsigned long)frequency.frequency_millihz,
           tone.amplitude_q15);
    return 0;
}