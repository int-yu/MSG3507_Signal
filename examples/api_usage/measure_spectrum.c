#include <stdint.h>
#include <stdio.h>

#include "sigq15.h"

static const int16_t sine16[16] = {
    0,12539,23170,30274,32767,30274,23170,12539,
    0,-12539,-23170,-30274,-32768,-30274,-23170,-12539
};

int main(void)
{
    int16_t frame[256];
    int16_t windowed[256];
    uint32_t power[16] = {0};
    sigq15_diag_t diag = {0};
    sigq15_stats_result_t stats;
    sigq15_frequency_result_t frequency;
    sigq15_goertzel_result_t tone;
    sigq15_correlation_result_t delay;
    sigq15_spectrum_metrics_t metrics;
    for (uint16_t i = 0; i < 256; ++i) frame[i] = (int16_t)(((int32_t)sine16[i & 15] * 16384) >> 15);
    sigq15_apply_window(frame, windowed, 256, SIGQ15_WINDOW_HANN, &diag);
    if (sigq15_stats(frame, 256, &stats) != SIGQ15_OK ||
        sigq15_frequency_zero_cross(frame, 256, 1600, 256, &frequency) != SIGQ15_OK ||
        sigq15_goertzel(windowed, 256, 1600, 100, &tone, &diag) != SIGQ15_OK ||
        sigq15_cross_correlate(frame, frame, 256, 8, &delay) != SIGQ15_OK)
        return 1;
    power[2] = tone.power_q30;
    power[4] = tone.power_q30 >> 8;
    if (sigq15_spectrum_metrics(power, 16, 2, 4, &metrics) != SIGQ15_OK) return 1;
    printf("rms_q15=%d freq_mHz=%lu amplitude_q15=%d snr_q8=%d delay_q16=%ld\n",
           stats.rms_q15, (unsigned long)frequency.frequency_millihz,
           tone.amplitude_q15, metrics.snr_q8_db, (long)delay.delay_q16_samples);
    return 0;
}
