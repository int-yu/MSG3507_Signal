#include <stdint.h>
#include <stdio.h>

#include "sigq15.h"

static const int16_t sine16[16] = {
    0,12539,23170,30274,32767,30274,23170,12539,
    0,-12539,-23170,-30274,-32768,-30274,-23170,-12539
};

int main(void)
{
    static int16_t weights[4];
    static int16_t history[4];
    sigq15_diag_t diag = {0};
    sigq31_nco_t nco;
    sigq15_iq_demod_t iq;
    sigq31_fll_t fll;
    sigq31_pll_t pll;
    sigq15_lms_t nlms;
    sigq15_agc_t display_agc;
    int16_t error = 0;
    if (sigq31_nco_init(&nco, 1600, 100, sine16, 16) != SIGQ15_OK ||
        sigq15_iq_init(&iq, 1600, 100, 2048, sine16, 16) != SIGQ15_OK ||
        sigq31_fll_init(&fll, 1600, 100000, 80000, 120000, 1024) != SIGQ15_OK ||
        sigq31_pll_init(&pll, 1600, 100000, 1 << 18, 1 << 8, 80000, 120000) != SIGQ15_OK ||
        sigq15_lms_init(&nlms, weights, history, 4, 1024, 1) != SIGQ15_OK ||
        sigq15_agc_init(&display_agc, 12000, 256, 64, 8192, 32767) != SIGQ15_OK)
        return 1;
    for (uint16_t n = 0; n < 256; ++n) {
        int16_t sample = (int16_t)(((int32_t)sine16[n & 15] * 12000) >> 15);
        int16_t s, c;
        sigq15_iq_result_t result;
        sigq31_nco_next(&nco, &s, &c);
        result = sigq15_iq_process(&iq, sample, &diag);
        (void)sigq31_fll_update(&fll, (int32_t)result.phase_q32);
        (void)sigq31_pll_process(&pll, sample, sine16, 16);
        (void)sigq15_lms_process(&nlms, sample, (int16_t)(sample >> 1), &error, &diag);
        (void)sigq15_agc_process(&display_agc, sample, &diag);
    }
    if (!sigq31_pll_locked(&pll)) sigq31_pll_reset(&pll, fll.frequency_millihz);
    sigq15_iq_reset(&iq);
    sigq15_lms_reset(&nlms);
    sigq15_agc_reset(&display_agc);
    printf("fll_mHz=%lu pll_mHz=%lu saturation=%lu\n", (unsigned long)fll.frequency_millihz,
           (unsigned long)pll.frequency_millihz, (unsigned long)diag.saturation_count);
    return 0;
}
