#include <stdint.h>
#include <stdio.h>

#include "sigq15.h"
#include "sigq15_backends.h"

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); return 1; \
} } while (0)

static int iabs32(int32_t x) { return x < 0 ? -x : x; }

static int test_numeric_and_filter(void)
{
    sigq15_diag_t diag = {0};
    CHECK(sigq15_sat(40000, &diag) == 32767);
    CHECK(diag.saturation_count == 1);
    CHECK(sigq31_phase_wrap((uint32_t)0x40000000U) == 0x40000000U);

    int16_t coeffs[3] = {8192, 16384, 8192};
    int16_t state[3] = {0};
    sigq15_fir_t fir;
    int16_t input[5] = {32767, 0, 0, 0, 0};
    int16_t output[5] = {0};
    CHECK(sigq15_fir_init(&fir, coeffs, 3, state, 3) == SIGQ15_OK);
    CHECK(sigq15_fir_process_block(&fir, input, output, 5, &diag) == SIGQ15_OK);
    CHECK(iabs32(output[0] - 8192) <= 1);
    CHECK(iabs32(output[1] - 16384) <= 1);
    CHECK(iabs32(output[2] - 8192) <= 1);
    return 0;
}

static int test_measure_and_goertzel(void)
{
    static const int16_t sine16[16] = {
        0, 12539, 23170, 30274, 32767, 30274, 23170, 12539,
        0, -12539, -23170, -30274, -32768, -30274, -23170, -12539
    };
    int16_t x[256];
    for (int i = 0; i < 256; ++i) x[i] = (int16_t)(((int32_t)sine16[i & 15] * 19660) >> 15);
    sigq15_stats_result_t stats;
    CHECK(sigq15_stats(x, 256, &stats) == SIGQ15_OK);
    CHECK(iabs32(stats.rms_q15 - 13901) < 80);

    sigq15_goertzel_result_t g;
    CHECK(sigq15_goertzel(x, 256, 1600, 100, &g, 0) == SIGQ15_OK);
    CHECK(g.valid && iabs32(g.amplitude_q15 - 19660) < 300);
    return 0;
}

static int test_nco_and_lms(void)
{
    static const int16_t sine16[16] = {
        0, 12539, 23170, 30274, 32767, 30274, 23170, 12539,
        0, -12539, -23170, -30274, -32768, -30274, -23170, -12539
    };
    sigq31_nco_t nco;
    CHECK(sigq31_nco_init(&nco, 1600, 100, sine16, 16) == SIGQ15_OK);
    int16_t s, c;
    sigq31_nco_next(&nco, &s, &c);
    CHECK(iabs32(s) <= 1 && iabs32(c - 32767) <= 1);

    int16_t weights[4] = {0};
    int16_t history[4] = {0};
    sigq15_lms_t lms;
    CHECK(sigq15_lms_init(&lms, weights, history, 4, 2048, 1) == SIGQ15_OK);
    int16_t error = 0;
    sigq15_diag_t diag = {0};
    for (int n = 0; n < 3000; ++n) {
        int16_t x = sine16[n & 15];
        int16_t d = (int16_t)(x >> 1);
        (void)sigq15_lms_process(&lms, x, d, &error, &diag);
    }
    CHECK(iabs32(error) < 1000);
    return 0;
}

static int test_preprocess_windows_and_decimator(void)
{
    int16_t x[5] = {100, 200, 300, 400, 500};
    sigq15_diag_t diag = {0};
    sigq15_remove_mean(x, 5);
    CHECK(x[0] == -200 && x[4] == 200);
    CHECK(sigq15_window_value(SIGQ15_WINDOW_HANN, 0, 17) == 0);
    CHECK(sigq15_window_value(SIGQ15_WINDOW_HANN, 8, 17) > 32760);
    int16_t state[4];
    sigq15_moving_average_t ma;
    CHECK(sigq15_moving_average_init(&ma, state, 4, 4) == SIGQ15_OK);
    CHECK(sigq15_moving_average_process(&ma, 4000, &diag) == 1000);
    CHECK(sigq15_moving_average_process(&ma, 4000, &diag) == 2000);
    int16_t coeff = 32767, fir_state = 0, output[2] = {0};
    sigq15_fir_t fir;
    sigq15_decimator_t dec;
    CHECK(sigq15_fir_init(&fir, &coeff, 1, &fir_state, 1) == SIGQ15_OK);
    CHECK(sigq15_decimator_init(&dec, &fir, 2) == SIGQ15_OK);
    CHECK(sigq15_decimator_process(&dec, (int16_t[]){1000,2000,3000,4000},
                                   4, output, 2, &diag) == 2);
    CHECK(iabs32(output[0] - 2000) <= 1 && iabs32(output[1] - 4000) <= 1);
    return 0;
}

static int test_correlation_spectrum_and_backends(void)
{
    int16_t a[16] = {0}, b[16] = {0};
    a[4] = 16000; b[7] = 16000;
    sigq15_correlation_result_t correlation;
    CHECK(sigq15_cross_correlate(a, b, 16, 5, &correlation) == SIGQ15_OK);
    CHECK(correlation.delay_q16_samples == (3 << 16));
    int16_t qa[24] = {0}, qb[24] = {0};
    qa[8] = 20000;
    qb[10] = 10000; qb[11] = 20000;
    CHECK(sigq15_cross_correlate(qa, qb, 24, 5, &correlation) == SIGQ15_OK);
    CHECK(correlation.delay_q16_samples > (2 << 16));
    CHECK(correlation.delay_q16_samples < (3 << 16));

    uint32_t power[16] = {0};
    power[2] = 1000000; power[4] = 10000; power[6] = 2500; power[3] = 100;
    sigq15_spectrum_metrics_t metrics;
    CHECK(sigq15_spectrum_metrics(power, 16, 2, 3, &metrics) == SIGQ15_OK);
    CHECK(metrics.valid && metrics.thd_q15 > 3000 && metrics.thd_q15 < 4000);
    CHECK(sigq15_backend_available() == SIGQ15_BACKEND_PORTABLE);
    CHECK(!sigq15_cmsis_available() && !sigq15_mathacl_available());
    CHECK(sigq15_cmsis_rfft_q15(a, 16, b) == SIGQ15_ERANGE);
    return 0;
}
static int test_db_fll_and_agc(void)
{
    uint32_t p[8] = {0};
    sigq15_spectrum_metrics_t m;
    p[1] = 1000000; p[2] = 10000;
    CHECK(sigq15_spectrum_metrics(p, 8, 1, 1, &m) == SIGQ15_OK);
    CHECK(iabs32(m.snr_q8_db - 5120) <= 4); /* 20.00 dB */
    p[2] = 100000;
    CHECK(sigq15_spectrum_metrics(p, 8, 1, 1, &m) == SIGQ15_OK);
    CHECK(iabs32(m.snr_q8_db - 2560) <= 4); /* 10.00 dB */
    p[2] = 100;
    CHECK(sigq15_spectrum_metrics(p, 8, 1, 1, &m) == SIGQ15_OK);
    CHECK(iabs32(m.snr_q8_db - 10240) <= 8); /* 40.00 dB */

    sigq31_fll_t fll;
    CHECK(sigq31_fll_init(&fll, 1600, 50000, 40000, 70000, 4096) == SIGQ15_OK);
    CHECK(sigq31_fll_update(&fll, 0x10000000U) > 50000);
    CHECK(sigq31_fll_update(&fll, (int32_t)0xf0000000U) < 70000);

    sigq15_agc_t agc;
    CHECK(sigq15_agc_init(&agc, 12000, 256, 64, 8192, 32767) == SIGQ15_OK);
    int16_t output = 0;
    for (int i = 0; i < 1000; ++i) output = sigq15_agc_process(&agc, 2000, 0);
    CHECK(output > 2000);
    sigq15_agc_reset(&agc);
    CHECK(agc.gain_q15 == 8192);
    return 0;
}

static int test_pll_stability_step_and_dropout(void)
{
    static const int16_t sine16[16] = {
        0,12539,23170,30274,32767,30274,23170,12539,
        0,-12539,-23170,-30274,-32768,-30274,-23170,-12539
    };
    sigq31_pll_t pll;
    CHECK(sigq31_pll_init(&pll, 1600, 50000, 1 << 18, 1 << 8,
                          40000, 70000) == SIGQ15_OK);
    uint32_t source_phase = 0;
    uint32_t step50 = (uint32_t)(((uint64_t)50 << 32) / 1600);
    for (int n = 0; n < 4000; ++n) {
        uint32_t idx = (uint32_t)(((uint64_t)source_phase * 16) >> 32);
        (void)sigq31_pll_process(&pll, sine16[idx & 15], sine16, 16);
        source_phase += step50;
    }
    CHECK(pll.frequency_millihz >= 49000 && pll.frequency_millihz <= 51000);
    uint32_t step55 = (uint32_t)(((uint64_t)55 << 32) / 1600);
    for (int n = 0; n < 4000; ++n) {
        uint32_t idx = (uint32_t)(((uint64_t)source_phase * 16) >> 32);
        (void)sigq31_pll_process(&pll, sine16[idx & 15], sine16, 16);
        source_phase += step55;
    }
    CHECK(pll.frequency_millihz >= 52000 && pll.frequency_millihz <= 58000);
    uint32_t before = pll.frequency_millihz;
    for (int n = 0; n < 1000; ++n)
        (void)sigq31_pll_process(&pll, 0, sine16, 16);
    CHECK(iabs32((int32_t)pll.frequency_millihz - (int32_t)before) < 1000);
    return 0;
}
static int test_iq_pll_transfer(void)
{
    static const int16_t sine16[16] = {
        0, 12539, 23170, 30274, 32767, 30274, 23170, 12539,
        0, -12539, -23170, -30274, -32768, -30274, -23170, -12539
    };
    int16_t x[256], y[256];
    sigq15_diag_t diag = {0};
    sigq15_iq_demod_t iq;
    sigq15_iq_result_t qr = {0};
    CHECK(sigq15_iq_init(&iq, 1600, 100, 2048, sine16, 16) == SIGQ15_OK);
    for (int n = 0; n < 256; ++n) {
        x[n] = (int16_t)(((int32_t)sine16[n & 15] * 12000) >> 15);
        y[n] = (int16_t)(x[n] >> 1);
        qr = sigq15_iq_process(&iq, x[n], &diag);
    }
    CHECK(qr.valid && iabs32(qr.amplitude_q15 - 12000) < 800);
    CHECK(sigq31_phase_to_millidegrees(qr.phase_q32) <= 360000U);
    sigq15_iq_reset(&iq);
    CHECK(iq.i_q30 == 0 && iq.q_q30 == 0);
    sigq15_transfer_result_t tr;
    CHECK(sigq15_transfer_point(x, y, 256, 1600, 100, &tr, &diag) == SIGQ15_OK);
    CHECK(iabs32(tr.magnitude_q20 - (1 << 19)) < 10000);
    CHECK(tr.coherence_q15 > 32000);
    for (int n = 0; n < 256; ++n) {
        x[n] >>= 2;
        y[n] = (int16_t)(x[n] << 1);
    }
    CHECK(sigq15_transfer_point(x, y, 256, 1600, 100, &tr, &diag) == SIGQ15_OK);
    CHECK(iabs32(tr.magnitude_q20 - (2 << 20)) < 30000);
    sigq31_pll_t pll;
    CHECK(sigq31_pll_init(&pll, 1600, 100000, 1 << 20, 1 << 12,
                          80000, 120000) == SIGQ15_OK);
    sigq31_pll_reset(&pll, 100000);
    CHECK(pll.phase == 0 && pll.frequency_millihz == 100000);
    return 0;
}

int main(void)
{
    CHECK(test_numeric_and_filter() == 0);
    CHECK(test_measure_and_goertzel() == 0);
    CHECK(test_nco_and_lms() == 0);
    CHECK(test_preprocess_windows_and_decimator() == 0);
    CHECK(test_correlation_spectrum_and_backends() == 0);
    CHECK(test_db_fll_and_agc() == 0);
    CHECK(test_pll_stability_step_and_dropout() == 0);
    CHECK(test_iq_pll_transfer() == 0);
    puts("MSPM0G3507 host tests: PASS");
    return 0;
}
