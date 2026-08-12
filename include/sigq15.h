#ifndef SIGQ15_H
#define SIGQ15_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SIGQ15_VERSION_MAJOR 1
#define SIGQ15_VERSION_MINOR 0
#define SIGQ15_VERSION_PATCH 0

typedef enum {
    SIGQ15_OK = 0, SIGQ15_EINVAL, SIGQ15_EWORKSPACE, SIGQ15_ENOSIGNAL,
    SIGQ15_ENOTLOCKED, SIGQ15_ENOTCONVERGED, SIGQ15_ERANGE
} sigq15_status_t;

typedef struct {
    uint32_t saturation_count, overflow_count, invalid_count;
    uint8_t valid, locked, converged;
} sigq15_diag_t;

int16_t sigq15_sat(int32_t value, sigq15_diag_t *diag);
int32_t sigq31_sat(int64_t value, sigq15_diag_t *diag);
uint32_t sigq31_phase_wrap(uint32_t phase);
uint32_t sigq31_phase_to_millidegrees(uint32_t phase);
int16_t sigq15_median3(int16_t a, int16_t b, int16_t c);
int16_t sigq15_linear_calibrate(int16_t sample, int16_t gain_q15,
                                int16_t offset_q15, sigq15_diag_t *diag);

typedef struct { int16_t alpha_q15, x1, y1; } sigq15_dc_blocker_t;
sigq15_status_t sigq15_dc_blocker_init(sigq15_dc_blocker_t *state, int16_t alpha_q15);
void sigq15_dc_blocker_reset(sigq15_dc_blocker_t *state);
int16_t sigq15_dc_blocker_process(sigq15_dc_blocker_t *state, int16_t input,
                                  sigq15_diag_t *diag);
void sigq15_remove_mean(int16_t *samples, size_t count);

typedef struct {
    int16_t *state;
    size_t length, index, filled;
    int64_t sum;
} sigq15_moving_average_t;
size_t sigq15_moving_average_workspace_size(size_t length);
sigq15_status_t sigq15_moving_average_init(sigq15_moving_average_t *filter,
                                           int16_t *state, size_t length,
                                           size_t state_count);
void sigq15_moving_average_reset(sigq15_moving_average_t *filter);
int16_t sigq15_moving_average_process(sigq15_moving_average_t *filter,
                                      int16_t input, sigq15_diag_t *diag);

typedef struct {
    const int16_t *coeffs;
    int16_t *state;
    size_t taps, index;
} sigq15_fir_t;
size_t sigq15_fir_workspace_size(size_t taps);
sigq15_status_t sigq15_fir_init(sigq15_fir_t *fir, const int16_t *coeffs,
                                size_t taps, int16_t *state, size_t state_count);
void sigq15_fir_reset(sigq15_fir_t *fir);
int16_t sigq15_fir_process(sigq15_fir_t *fir, int16_t input, sigq15_diag_t *diag);
sigq15_status_t sigq15_fir_process_block(sigq15_fir_t *fir, const int16_t *input,
                                         int16_t *output, size_t count,
                                         sigq15_diag_t *diag);

typedef struct { sigq15_fir_t *fir; uint16_t factor, phase; } sigq15_decimator_t;
sigq15_status_t sigq15_decimator_init(sigq15_decimator_t *decimator,
                                      sigq15_fir_t *anti_alias_fir,
                                      uint16_t factor);
void sigq15_decimator_reset(sigq15_decimator_t *decimator);
size_t sigq15_decimator_process(sigq15_decimator_t *decimator,
                                const int16_t *input, size_t input_count,
                                int16_t *output, size_t output_capacity,
                                sigq15_diag_t *diag);

typedef struct { int16_t b0,b1,b2,a1,a2; int8_t post_shift; } sigq15_biquad_coeffs_t;
typedef struct { int32_t d1,d2; } sigq15_biquad_state_t;
typedef struct { const sigq15_biquad_coeffs_t *coeffs; sigq15_biquad_state_t *state; size_t stages; }
    sigq15_biquad_t;
size_t sigq15_biquad_workspace_size(size_t stages);
sigq15_status_t sigq15_biquad_init(sigq15_biquad_t *filter,
                                   const sigq15_biquad_coeffs_t *coeffs,
                                   size_t stages, sigq15_biquad_state_t *state,
                                   size_t state_count);
void sigq15_biquad_reset(sigq15_biquad_t *filter);
int16_t sigq15_biquad_process(sigq15_biquad_t *filter, int16_t input,
                              sigq15_diag_t *diag);

typedef struct { int16_t mean_q15,rms_q15,minimum_q15,maximum_q15,peak_q15; uint8_t valid; }
    sigq15_stats_result_t;
sigq15_status_t sigq15_stats(const int16_t *input, size_t count,
                             sigq15_stats_result_t *result);

typedef struct { uint32_t frequency_millihz, period_q16_samples, crossings; uint8_t valid; }
    sigq15_frequency_result_t;
sigq15_status_t sigq15_frequency_zero_cross(const int16_t *input, size_t count,
                                            uint32_t sample_rate_hz, int16_t threshold_q15,
                                            sigq15_frequency_result_t *result);

typedef struct {
    int32_t correlation_q30;
    int32_t delay_q16_samples;
    uint32_t phase_q32;
    uint8_t valid;
} sigq15_correlation_result_t;
sigq15_status_t sigq15_cross_correlate(const int16_t *a, const int16_t *b,
                                       size_t count, int16_t max_lag,
                                       sigq15_correlation_result_t *result);

typedef enum { SIGQ15_WINDOW_RECT=0,SIGQ15_WINDOW_HANN,SIGQ15_WINDOW_BLACKMAN_HARRIS,
               SIGQ15_WINDOW_FLAT_TOP } sigq15_window_t;
int16_t sigq15_window_value(sigq15_window_t window, size_t index, size_t count);
void sigq15_apply_window(const int16_t *input, int16_t *output, size_t count,
                         sigq15_window_t window, sigq15_diag_t *diag);
uint16_t sigq15_window_coherent_gain_q15(sigq15_window_t window);
uint16_t sigq15_window_enbw_q12(sigq15_window_t window);

typedef struct { int32_t real_q15,imag_q15,power_q30; int16_t amplitude_q15; uint32_t phase_q32; uint8_t valid; }
    sigq15_goertzel_result_t;
sigq15_status_t sigq15_goertzel(const int16_t *input, size_t count,
                                uint32_t sample_rate_hz, uint32_t target_hz,
                                sigq15_goertzel_result_t *result, sigq15_diag_t *diag);
sigq15_status_t sigq15_goertzel_multi(const int16_t *input, size_t count,
                                      uint32_t sample_rate_hz, const uint32_t *targets_hz,
                                      sigq15_goertzel_result_t *results, size_t target_count,
                                      sigq15_diag_t *diag);

typedef struct { uint32_t phase, step; const int16_t *sine_table; uint16_t table_size; }
    sigq31_nco_t;
sigq15_status_t sigq31_nco_init(sigq31_nco_t *nco, uint32_t sample_rate_hz,
                                uint32_t frequency_hz, const int16_t *sine_table,
                                uint16_t table_size);
sigq15_status_t sigq31_nco_set_frequency(sigq31_nco_t *nco, uint32_t sample_rate_hz,
                                         uint32_t frequency_hz);
void sigq31_nco_next(sigq31_nco_t *nco, int16_t *sine, int16_t *cosine);

typedef struct {
    int16_t target_q15, attack_q15, release_q15;
    int16_t min_gain_q15, max_gain_q15, gain_q15;
} sigq15_agc_t;
sigq15_status_t sigq15_agc_init(sigq15_agc_t *agc, int16_t target_q15,
                                int16_t attack_q15, int16_t release_q15,
                                int16_t min_gain_q15, int16_t max_gain_q15);
void sigq15_agc_reset(sigq15_agc_t *agc);
int16_t sigq15_agc_process(sigq15_agc_t *agc, int16_t input,
                           sigq15_diag_t *diag);
typedef struct { sigq31_nco_t nco; int16_t alpha_q15; int32_t i_q30,q_q30; }
    sigq15_iq_demod_t;
typedef struct { int16_t i_q15,q_q15,amplitude_q15; uint32_t phase_q32; uint8_t valid; }
    sigq15_iq_result_t;
sigq15_status_t sigq15_iq_init(sigq15_iq_demod_t *demod, uint32_t sample_rate_hz,
                               uint32_t carrier_hz, int16_t alpha_q15,
                               const int16_t *sine_table, uint16_t table_size);
void sigq15_iq_reset(sigq15_iq_demod_t *demod);
sigq15_iq_result_t sigq15_iq_process(sigq15_iq_demod_t *demod, int16_t sample,
                                     sigq15_diag_t *diag);

typedef struct { int16_t *weights,*history; size_t taps,index; int16_t mu_q15; uint8_t normalized; }
    sigq15_lms_t;
size_t sigq15_lms_workspace_size(size_t taps);
sigq15_status_t sigq15_lms_init(sigq15_lms_t *lms, int16_t *weights, int16_t *history,
                                size_t taps, int16_t mu_q15, uint8_t normalized);
void sigq15_lms_reset(sigq15_lms_t *lms);
int16_t sigq15_lms_process(sigq15_lms_t *lms, int16_t reference, int16_t desired,
                           int16_t *error, sigq15_diag_t *diag);

typedef struct {
    uint32_t sample_rate_hz, frequency_millihz, center_millihz;
    uint32_t min_millihz, max_millihz;
    int16_t gain_q15;
} sigq31_fll_t;
sigq15_status_t sigq31_fll_init(sigq31_fll_t *fll, uint32_t sample_rate_hz,
                                uint32_t initial_millihz,
                                uint32_t min_millihz, uint32_t max_millihz,
                                int16_t gain_q15);
uint32_t sigq31_fll_update(sigq31_fll_t *fll, int32_t phase_error_q32);
typedef struct { uint32_t sample_rate_hz, phase, frequency_millihz; int32_t kp_q30,ki_q30; int64_t integrator_q30; uint32_t min_millihz,max_millihz,lock_metric; uint16_t lock_count;
    int16_t previous_sample; uint32_t samples_since_crossing; }
    sigq31_pll_t;
sigq15_status_t sigq31_pll_init(sigq31_pll_t *pll, uint32_t sample_rate_hz,
                                uint32_t initial_millihz, int32_t kp_q30, int32_t ki_q30,
                                uint32_t min_millihz, uint32_t max_millihz);
void sigq31_pll_reset(sigq31_pll_t *pll, uint32_t frequency_millihz);
int16_t sigq31_pll_process(sigq31_pll_t *pll, int16_t sample,
                           const int16_t *sine_table, uint16_t table_size);
uint8_t sigq31_pll_locked(const sigq31_pll_t *pll);

typedef struct { int32_t real_q20,imag_q20,magnitude_q20; uint32_t phase_q32; uint16_t coherence_q15; uint8_t valid; }
    sigq15_transfer_result_t;
sigq15_status_t sigq15_transfer_point(const int16_t *input, const int16_t *output,
                                      size_t count, uint32_t sample_rate_hz,
                                      uint32_t frequency_hz,
                                      sigq15_transfer_result_t *result,
                                      sigq15_diag_t *diag);

typedef struct {
    uint16_t thd_q15, thdn_q15;
    int16_t snr_q8_db, sinad_q8_db, sfdr_q8_db;
    uint32_t noise_power_q30;
    uint8_t valid;
} sigq15_spectrum_metrics_t;
sigq15_status_t sigq15_spectrum_metrics(const uint32_t *power_q30,
                                        size_t bins, size_t fundamental_bin,
                                        size_t harmonic_count,
                                        sigq15_spectrum_metrics_t *result);

/* Optional platform adapters. The portable core never requires either SDK. */
typedef enum { SIGQ15_BACKEND_PORTABLE=0, SIGQ15_BACKEND_CMSIS,
               SIGQ15_BACKEND_MATHACL } sigq15_backend_t;
sigq15_backend_t sigq15_backend_available(void);

#ifdef __cplusplus
}
#endif
#endif
