#include "sigq15.h"

#include <limits.h>
#include <string.h>

static const uint32_t cordic_atan_q32[16] = {
    536870912U, 316933406U, 167458907U, 85004756U,
    42667331U, 21354465U, 10679838U, 5340245U,
    2670163U, 1335087U, 667544U, 333772U,
    166886U, 83443U, 41722U, 20861U
};

static uint32_t isqrt64(uint64_t value)
{
    uint64_t result = 0;
    uint64_t bit = (uint64_t)1 << 62;
    while (bit > value) bit >>= 2;
    while (bit != 0) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return (uint32_t)result;
}

static void sincos_q30(uint32_t phase, int32_t *sine, int32_t *cosine)
{
    uint32_t quadrant = phase >> 30;
    uint32_t local = phase & 0x3fffffffU;
    int32_t angle = (int32_t)((quadrant & 1U) ? 0x40000000U - local : local);
    int64_t x = 652032874;
    int64_t y = 0;
    for (unsigned i = 0; i < 16; ++i) {
        int64_t old_x = x;
        if (angle >= 0) {
            x -= y >> i;
            y += old_x >> i;
            angle -= (int32_t)cordic_atan_q32[i];
        } else {
            x += y >> i;
            y -= old_x >> i;
            angle += (int32_t)cordic_atan_q32[i];
        }
    }
    if (quadrant == 1U || quadrant == 2U) x = -x;
    if (quadrant >= 2U) y = -y;
    *sine = (int32_t)y;
    *cosine = (int32_t)x;
}

static uint32_t atan2_q32(int64_t y, int64_t x)
{
    if (x == 0 && y == 0) return 0;
    uint32_t base = 0;
    if (x < 0) {
        x = -x;
        y = -y;
        base = 0x80000000U;
    }
    int32_t angle = 0;
    for (unsigned i = 0; i < 16; ++i) {
        int64_t old_x = x;
        if (y > 0) {
            x += y >> i;
            y -= old_x >> i;
            angle += (int32_t)cordic_atan_q32[i];
        } else {
            x -= y >> i;
            y += old_x >> i;
            angle -= (int32_t)cordic_atan_q32[i];
        }
    }
    return base + (uint32_t)angle;
}

int16_t sigq15_sat(int32_t value, sigq15_diag_t *diag)
{
    if (value > INT16_MAX) {
        if (diag) ++diag->saturation_count;
        return INT16_MAX;
    }
    if (value < INT16_MIN) {
        if (diag) ++diag->saturation_count;
        return INT16_MIN;
    }
    return (int16_t)value;
}

int32_t sigq31_sat(int64_t value, sigq15_diag_t *diag)
{
    if (value > INT32_MAX) {
        if (diag) ++diag->saturation_count;
        return INT32_MAX;
    }
    if (value < INT32_MIN) {
        if (diag) ++diag->saturation_count;
        return INT32_MIN;
    }
    return (int32_t)value;
}

uint32_t sigq31_phase_wrap(uint32_t phase) { return phase; }
uint32_t sigq31_phase_to_millidegrees(uint32_t phase)
{
    return (uint32_t)(((uint64_t)phase * 360000U) >> 32);
}

int16_t sigq15_median3(int16_t a, int16_t b, int16_t c)
{
    if (a > b) { int16_t t = a; a = b; b = t; }
    if (b > c) { int16_t t = b; b = c; c = t; }
    return a > b ? a : b;
}

int16_t sigq15_linear_calibrate(int16_t x, int16_t gain, int16_t offset,
                                sigq15_diag_t *diag)
{
    return sigq15_sat((((int32_t)x * gain + 16384) >> 15) + offset, diag);
}

sigq15_status_t sigq15_dc_blocker_init(sigq15_dc_blocker_t *state, int16_t alpha)
{
    if (!state || alpha < 0) return SIGQ15_EINVAL;
    state->alpha_q15 = alpha;
    state->x1 = state->y1 = 0;
    return SIGQ15_OK;
}

void sigq15_dc_blocker_reset(sigq15_dc_blocker_t *state)
{
    if (state) state->x1 = state->y1 = 0;
}

int16_t sigq15_dc_blocker_process(sigq15_dc_blocker_t *state, int16_t input,
                                  sigq15_diag_t *diag)
{
    if (!state) {
        if (diag) ++diag->invalid_count;
        return 0;
    }
    int32_t y = (int32_t)input - state->x1 +
                (((int32_t)state->alpha_q15 * state->y1) >> 15);
    state->x1 = input;
    state->y1 = sigq15_sat(y, diag);
    if (diag) diag->valid = 1;
    return state->y1;
}

void sigq15_remove_mean(int16_t *samples, size_t count)
{
    if (!samples || !count) return;
    int64_t sum = 0;
    for (size_t i = 0; i < count; ++i) sum += samples[i];
    int32_t mean = (int32_t)(sum / (int64_t)count);
    for (size_t i = 0; i < count; ++i) samples[i] = (int16_t)(samples[i] - mean);
}

size_t sigq15_moving_average_workspace_size(size_t length)
{
    return length * sizeof(int16_t);
}

sigq15_status_t sigq15_moving_average_init(sigq15_moving_average_t *filter,
                                           int16_t *state, size_t length,
                                           size_t state_count)
{
    if (!filter || !state || !length) return SIGQ15_EINVAL;
    if (state_count < length) return SIGQ15_EWORKSPACE;
    filter->state = state;
    filter->length = length;
    filter->index = filter->filled = 0;
    filter->sum = 0;
    memset(state, 0, length * sizeof(*state));
    return SIGQ15_OK;
}

void sigq15_moving_average_reset(sigq15_moving_average_t *filter)
{
    if (!filter) return;
    memset(filter->state, 0, filter->length * sizeof(*filter->state));
    filter->index = filter->filled = 0;
    filter->sum = 0;
}

int16_t sigq15_moving_average_process(sigq15_moving_average_t *filter,
                                      int16_t input, sigq15_diag_t *diag)
{
    if (!filter) {
        if (diag) ++diag->invalid_count;
        return 0;
    }
    filter->sum -= filter->state[filter->index];
    filter->state[filter->index] = input;
    filter->sum += input;
    filter->index = (filter->index + 1) % filter->length;
    if (filter->filled < filter->length) ++filter->filled;
    return sigq15_sat((int32_t)(filter->sum / (int64_t)filter->length), diag);
}

size_t sigq15_fir_workspace_size(size_t taps) { return taps * sizeof(int16_t); }

sigq15_status_t sigq15_fir_init(sigq15_fir_t *fir, const int16_t *coeffs,
                                size_t taps, int16_t *state, size_t state_count)
{
    if (!fir || !coeffs || !state || !taps) return SIGQ15_EINVAL;
    if (state_count < taps) return SIGQ15_EWORKSPACE;
    fir->coeffs = coeffs;
    fir->state = state;
    fir->taps = taps;
    fir->index = 0;
    memset(state, 0, taps * sizeof(*state));
    return SIGQ15_OK;
}

void sigq15_fir_reset(sigq15_fir_t *fir)
{
    if (!fir) return;
    memset(fir->state, 0, fir->taps * sizeof(*fir->state));
    fir->index = 0;
}

int16_t sigq15_fir_process(sigq15_fir_t *fir, int16_t input, sigq15_diag_t *diag)
{
    if (!fir) {
        if (diag) ++diag->invalid_count;
        return 0;
    }
    fir->state[fir->index] = input;
    int64_t acc = 0;
    size_t p = fir->index;
    for (size_t k = 0; k < fir->taps; ++k) {
        acc += (int32_t)fir->coeffs[k] * fir->state[p];
        p = p ? p - 1 : fir->taps - 1;
    }
    fir->index = (fir->index + 1) % fir->taps;
    return sigq15_sat((int32_t)((acc + 16384) >> 15), diag);
}

sigq15_status_t sigq15_fir_process_block(sigq15_fir_t *fir, const int16_t *input,
                                         int16_t *output, size_t count,
                                         sigq15_diag_t *diag)
{
    if (!fir || !input || !output) return SIGQ15_EINVAL;
    for (size_t i = 0; i < count; ++i)
        output[i] = sigq15_fir_process(fir, input[i], diag);
    if (diag) diag->valid = 1;
    return SIGQ15_OK;
}

sigq15_status_t sigq15_decimator_init(sigq15_decimator_t *decimator,
                                      sigq15_fir_t *fir, uint16_t factor)
{
    if (!decimator || !fir || factor < 2) return SIGQ15_EINVAL;
    decimator->fir = fir;
    decimator->factor = factor;
    decimator->phase = 0;
    return SIGQ15_OK;
}

void sigq15_decimator_reset(sigq15_decimator_t *decimator)
{
    if (!decimator) return;
    decimator->phase = 0;
    sigq15_fir_reset(decimator->fir);
}

size_t sigq15_decimator_process(sigq15_decimator_t *decimator,
                                const int16_t *input, size_t input_count,
                                int16_t *output, size_t capacity,
                                sigq15_diag_t *diag)
{
    if (!decimator || !input || (!output && capacity)) return 0;
    size_t produced = 0;
    for (size_t i = 0; i < input_count; ++i) {
        int16_t y = sigq15_fir_process(decimator->fir, input[i], diag);
        if (++decimator->phase == decimator->factor) {
            decimator->phase = 0;
            if (produced < capacity) output[produced++] = y;
        }
    }
    return produced;
}

size_t sigq15_biquad_workspace_size(size_t stages)
{
    return stages * sizeof(sigq15_biquad_state_t);
}

sigq15_status_t sigq15_biquad_init(sigq15_biquad_t *filter,
                                   const sigq15_biquad_coeffs_t *coeffs,
                                   size_t stages, sigq15_biquad_state_t *state,
                                   size_t state_count)
{
    if (!filter || !coeffs || !state || !stages) return SIGQ15_EINVAL;
    if (state_count < stages) return SIGQ15_EWORKSPACE;
    for (size_t i = 0; i < stages; ++i)
        if (coeffs[i].post_shift < 0 || coeffs[i].post_shift > 14)
            return SIGQ15_ERANGE;
    filter->coeffs = coeffs;
    filter->state = state;
    filter->stages = stages;
    memset(state, 0, stages * sizeof(*state));
    return SIGQ15_OK;
}

void sigq15_biquad_reset(sigq15_biquad_t *filter)
{
    if (filter) memset(filter->state, 0, filter->stages * sizeof(*filter->state));
}

int16_t sigq15_biquad_process(sigq15_biquad_t *filter, int16_t input,
                              sigq15_diag_t *diag)
{
    if (!filter) return 0;
    for (size_t i = 0; i < filter->stages; ++i) {
        const sigq15_biquad_coeffs_t *c = &filter->coeffs[i];
        sigq15_biquad_state_t *s = &filter->state[i];
        int64_t acc = (int32_t)c->b0 * input + s->d1;
        int16_t y = sigq15_sat((int32_t)(acc >> (15 - c->post_shift)), diag);
        s->d1 = sigq31_sat((int64_t)c->b1 * input - (int64_t)c->a1 * y + s->d2, diag);
        s->d2 = sigq31_sat((int64_t)c->b2 * input - (int64_t)c->a2 * y, diag);
        input = y;
    }
    return input;
}

sigq15_status_t sigq15_stats(const int16_t *input, size_t count,
                             sigq15_stats_result_t *result)
{
    if (!input || !result || !count) return SIGQ15_EINVAL;
    int64_t sum = 0;
    uint64_t squares = 0;
    int16_t minimum = input[0], maximum = input[0], peak = 0;
    for (size_t i = 0; i < count; ++i) {
        sum += input[i];
        squares += (uint64_t)((int32_t)input[i] * input[i]);
        if (input[i] < minimum) minimum = input[i];
        if (input[i] > maximum) maximum = input[i];
        int32_t a = input[i] < 0 ? -(int32_t)input[i] : input[i];
        if (a > peak) peak = (int16_t)(a > INT16_MAX ? INT16_MAX : a);
    }
    result->mean_q15 = (int16_t)(sum / (int64_t)count);
    result->rms_q15 = (int16_t)isqrt64(squares / count);
    result->minimum_q15 = minimum;
    result->maximum_q15 = maximum;
    result->peak_q15 = peak;
    result->valid = result->rms_q15 != 0;
    return result->valid ? SIGQ15_OK : SIGQ15_ENOSIGNAL;
}

sigq15_status_t sigq15_frequency_zero_cross(const int16_t *input, size_t count,
                                            uint32_t sample_rate_hz,
                                            int16_t threshold,
                                            sigq15_frequency_result_t *result)
{
    if (!input || !result || count < 3 || !sample_rate_hz || threshold < 0)
        return SIGQ15_EINVAL;
    uint64_t first = 0, last = 0;
    uint32_t crossings = 0;
    for (size_t i = 1; i < count; ++i) {
        int32_t delta = (int32_t)input[i] - input[i - 1];
        if (input[i - 1] <= 0 && input[i] > 0 && delta >= threshold) {
            uint32_t fraction = (uint32_t)((-(int64_t)input[i - 1] << 16) / delta);
            uint64_t position = ((uint64_t)(i - 1) << 16) + fraction;
            if (!crossings) first = position;
            last = position;
            ++crossings;
        }
    }
    result->crossings = crossings;
    result->valid = crossings >= 2;
    if (!result->valid) return SIGQ15_ENOSIGNAL;
    result->period_q16_samples = (uint32_t)((last - first) / (crossings - 1));
    result->frequency_millihz =
        (uint32_t)(((uint64_t)sample_rate_hz * 1000U << 16) /
                   result->period_q16_samples);
    return SIGQ15_OK;
}

sigq15_status_t sigq15_cross_correlate(const int16_t *a, const int16_t *b,
                                       size_t count, int16_t max_lag,
                                       sigq15_correlation_result_t *result)
{
    if (!a || !b || !result || !count || max_lag < 0) return SIGQ15_EINVAL;
    int16_t best_lag = 0;
    int64_t best = INT64_MIN, left = INT64_MIN, right = INT64_MIN;
    for (int16_t lag = (int16_t)-max_lag; lag <= max_lag; ++lag) {
        int64_t sum = 0;
        for (size_t i = 0; i < count; ++i) {
            int32_t j = (int32_t)i + lag;
            if (j >= 0 && (size_t)j < count) sum += (int32_t)a[i] * b[j];
        }
        if (sum > best) { best = sum; best_lag = lag; }
    }
    if (best_lag > -max_lag && best_lag < max_lag) {
        for (size_t i = 0; i < count; ++i) {
            int32_t jl = (int32_t)i + best_lag - 1;
            int32_t jr = (int32_t)i + best_lag + 1;
            if (jl >= 0 && (size_t)jl < count) {
                if (left == INT64_MIN) left = 0;
                left += (int32_t)a[i] * b[jl];
            }
            if (jr >= 0 && (size_t)jr < count) {
                if (right == INT64_MIN) right = 0;
                right += (int32_t)a[i] * b[jr];
            }
        }
    }
    int32_t fractional_q16 = 0;
    if (left != INT64_MIN && right != INT64_MIN) {
        int64_t denominator = left - 2 * best + right;
        int64_t numerator = left - right;
        if (denominator != 0) {
            int64_t fraction = (numerator << 15) / denominator;
            if (fraction < -32768) fraction = -32768;
            if (fraction > 32768) fraction = 32768;
            fractional_q16 = (int32_t)fraction;
        }
    }
    result->correlation_q30 = sigq31_sat(best, 0);
    result->delay_q16_samples = (int32_t)best_lag * 65536 + fractional_q16;
    result->phase_q32 = 0;
    result->valid = best != 0;
    return result->valid ? SIGQ15_OK : SIGQ15_ENOSIGNAL;
}
int16_t sigq15_window_value(sigq15_window_t window, size_t index, size_t count)
{
    if (count <= 1) return INT16_MAX;
    uint32_t p = (uint32_t)(((uint64_t)index << 32) / (count - 1));
    int32_t s1, c1, s2, c2, s3, c3, s4, c4;
    sincos_q30(p, &s1, &c1);
    sincos_q30(p * 2U, &s2, &c2);
    sincos_q30(p * 3U, &s3, &c3);
    sincos_q30(p * 4U, &s4, &c4);
    (void)s1; (void)s2; (void)s3; (void)s4;
    int64_t value = (int64_t)1 << 30;
    if (window == SIGQ15_WINDOW_HANN)
        value = 536870912LL - (c1 >> 1);
    else if (window == SIGQ15_WINDOW_BLACKMAN_HARRIS)
        value = 385204692LL - ((int64_t)524297245 * c1 >> 30)
              + ((int64_t)151698769 * c2 >> 30)
              - ((int64_t)12541305 * c3 >> 30);
    else if (window == SIGQ15_WINDOW_FLAT_TOP)
        value = 231476314LL - ((int64_t)447140898 * c1 >> 30)
              + ((int64_t)297709027 * c2 >> 30)
              - ((int64_t)89742213 * c3 >> 30)
              + ((int64_t)7459664 * c4 >> 30);
    return sigq15_sat((int32_t)((value + 16384) >> 15), 0);
}

void sigq15_apply_window(const int16_t *input, int16_t *output, size_t count,
                         sigq15_window_t window, sigq15_diag_t *diag)
{
    if (!input || !output) return;
    for (size_t i = 0; i < count; ++i)
        output[i] = sigq15_sat(((int32_t)input[i] *
                 sigq15_window_value(window, i, count) + 16384) >> 15, diag);
}

uint16_t sigq15_window_coherent_gain_q15(sigq15_window_t window)
{
    static const uint16_t gain[] = {32767, 16384, 11755, 7064};
    return window <= SIGQ15_WINDOW_FLAT_TOP ? gain[window] : 0;
}

uint16_t sigq15_window_enbw_q12(sigq15_window_t window)
{
    static const uint16_t enbw[] = {4096, 6144, 8215, 15442};
    return window <= SIGQ15_WINDOW_FLAT_TOP ? enbw[window] : 0;
}

sigq15_status_t sigq15_goertzel(const int16_t *input, size_t count,
                                uint32_t sample_rate_hz, uint32_t target_hz,
                                sigq15_goertzel_result_t *result,
                                sigq15_diag_t *diag)
{
    if (!input || !result || !count || !sample_rate_hz ||
        target_hz > sample_rate_hz / 2) return SIGQ15_EINVAL;
    uint32_t phase = (uint32_t)(((uint64_t)target_hz << 32) / sample_rate_hz);
    int32_t sine, cosine;
    sincos_q30(phase, &sine, &cosine);
    int32_t coefficient_q14 = cosine >> 15; /* 2*cos(w) in Q2.14 */
    int64_t s1 = 0, s2 = 0;
    for (size_t i = 0; i < count; ++i) {
        int64_t state = input[i] + ((coefficient_q14 * s1) >> 14) - s2;
        if (state > INT32_MAX || state < INT32_MIN) {
            if (diag) ++diag->overflow_count;
            state = state > 0 ? INT32_MAX : INT32_MIN;
        }
        s2 = s1;
        s1 = state;
    }
    int64_t real = s1 - ((s2 * cosine) >> 30);
    int64_t imag = (s2 * sine) >> 30;
    uint64_t power = (uint64_t)(real * real) + (uint64_t)(imag * imag);
    uint32_t magnitude = isqrt64(power);
    result->real_q15 = sigq31_sat(real, diag);
    result->imag_q15 = sigq31_sat(imag, diag);
    result->power_q30 = power > INT32_MAX ? INT32_MAX : (int32_t)power;
    result->amplitude_q15 =
        sigq15_sat((int32_t)(((uint64_t)magnitude * 2U + count / 2U) / count), diag);
    result->phase_q32 = atan2_q32(imag, real);
    result->valid = magnitude != 0;
    if (diag) diag->valid = result->valid;
    return result->valid ? SIGQ15_OK : SIGQ15_ENOSIGNAL;
}

sigq15_status_t sigq15_goertzel_multi(const int16_t *input, size_t count,
                                      uint32_t sample_rate_hz,
                                      const uint32_t *targets_hz,
                                      sigq15_goertzel_result_t *results,
                                      size_t target_count,
                                      sigq15_diag_t *diag)
{
    if (!input || !targets_hz || !results || !target_count) return SIGQ15_EINVAL;
    sigq15_status_t status = SIGQ15_OK;
    for (size_t i = 0; i < target_count; ++i) {
        sigq15_status_t current = sigq15_goertzel(input, count, sample_rate_hz,
                                                  targets_hz[i], &results[i], diag);
        if (current != SIGQ15_OK) status = current;
    }
    return status;
}

sigq15_status_t sigq31_nco_init(sigq31_nco_t *nco, uint32_t sample_rate_hz,
                                uint32_t frequency_hz,
                                const int16_t *table, uint16_t table_size)
{
    if (!nco || !sample_rate_hz || frequency_hz > sample_rate_hz / 2 ||
        !table || table_size < 4 || (table_size & (table_size - 1)))
        return SIGQ15_EINVAL;
    nco->phase = 0;
    nco->sine_table = table;
    nco->table_size = table_size;
    return sigq31_nco_set_frequency(nco, sample_rate_hz, frequency_hz);
}

sigq15_status_t sigq31_nco_set_frequency(sigq31_nco_t *nco,
                                         uint32_t sample_rate_hz,
                                         uint32_t frequency_hz)
{
    if (!nco || !sample_rate_hz || frequency_hz > sample_rate_hz / 2)
        return SIGQ15_ERANGE;
    nco->step = (uint32_t)(((uint64_t)frequency_hz << 32) / sample_rate_hz);
    return SIGQ15_OK;
}

void sigq31_nco_next(sigq31_nco_t *nco, int16_t *sine, int16_t *cosine)
{
    if (!nco || !sine || !cosine) return;
    uint32_t index = (uint32_t)(((uint64_t)nco->phase * nco->table_size) >> 32);
    uint32_t quarter = nco->table_size / 4;
    *sine = nco->sine_table[index & (nco->table_size - 1)];
    *cosine = nco->sine_table[(index + quarter) & (nco->table_size - 1)];
    nco->phase += nco->step;
}

sigq15_status_t sigq15_agc_init(sigq15_agc_t *agc, int16_t target,
                                int16_t attack, int16_t release,
                                int16_t min_gain, int16_t max_gain)
{
    if (!agc || target <= 0 || attack <= 0 || release <= 0 ||
        min_gain <= 0 || min_gain > max_gain) return SIGQ15_EINVAL;
    agc->target_q15 = target;
    agc->attack_q15 = attack;
    agc->release_q15 = release;
    agc->min_gain_q15 = min_gain;
    agc->max_gain_q15 = max_gain;
    agc->gain_q15 = min_gain;
    return SIGQ15_OK;
}
void sigq15_agc_reset(sigq15_agc_t *agc)
{
    if (agc) agc->gain_q15 = agc->min_gain_q15;
}
int16_t sigq15_agc_process(sigq15_agc_t *agc, int16_t input,
                           sigq15_diag_t *diag)
{
    if (!agc) return 0;
    int32_t magnitude = input < 0 ? -(int32_t)input : input;
    int32_t output_magnitude = (magnitude * agc->gain_q15) >> 14;
    int32_t error = agc->target_q15 - output_magnitude;
    int16_t rate = error < 0 ? agc->attack_q15 : agc->release_q15;
    int32_t gain = agc->gain_q15 + ((error * rate) >> 15);
    if (gain < agc->min_gain_q15) gain = agc->min_gain_q15;
    if (gain > agc->max_gain_q15) gain = agc->max_gain_q15;
    agc->gain_q15 = (int16_t)gain;
    return sigq15_sat(((int32_t)input * agc->gain_q15) >> 14, diag);
}
sigq15_status_t sigq15_iq_init(sigq15_iq_demod_t *demod,
                               uint32_t sample_rate_hz, uint32_t carrier_hz,
                               int16_t alpha_q15, const int16_t *table,
                               uint16_t table_size)
{
    if (!demod || alpha_q15 <= 0) return SIGQ15_EINVAL;
    sigq15_status_t status = sigq31_nco_init(&demod->nco, sample_rate_hz,
                                             carrier_hz, table, table_size);
    demod->alpha_q15 = alpha_q15;
    demod->i_q30 = demod->q_q30 = 0;
    return status;
}

void sigq15_iq_reset(sigq15_iq_demod_t *demod)
{
    if (!demod) return;
    demod->nco.phase = 0;
    demod->i_q30 = demod->q_q30 = 0;
}

sigq15_iq_result_t sigq15_iq_process(sigq15_iq_demod_t *demod, int16_t sample,
                                     sigq15_diag_t *diag)
{
    sigq15_iq_result_t result = {0};
    if (!demod) {
        if (diag) ++diag->invalid_count;
        return result;
    }
    int16_t sine, cosine;
    sigq31_nco_next(&demod->nco, &sine, &cosine);
    int32_t raw_i = (int32_t)sample * cosine;
    int32_t raw_q = -(int32_t)sample * sine;
    demod->i_q30 += ((int64_t)demod->alpha_q15 *
                     (raw_i - demod->i_q30)) >> 15;
    demod->q_q30 += ((int64_t)demod->alpha_q15 *
                     (raw_q - demod->q_q30)) >> 15;
    result.i_q15 = sigq15_sat(demod->i_q30 >> 15, diag);
    result.q_q15 = sigq15_sat(demod->q_q30 >> 15, diag);
    uint32_t magnitude = isqrt64((uint64_t)((int64_t)result.i_q15 * result.i_q15) +
                                 (uint64_t)((int64_t)result.q_q15 * result.q_q15));
    result.amplitude_q15 = sigq15_sat((int32_t)magnitude * 2, diag);
    result.phase_q32 = atan2_q32(result.q_q15, result.i_q15);
    result.valid = result.amplitude_q15 != 0;
    if (diag) diag->valid = result.valid;
    return result;
}

size_t sigq15_lms_workspace_size(size_t taps)
{
    return 2 * taps * sizeof(int16_t);
}

sigq15_status_t sigq15_lms_init(sigq15_lms_t *lms, int16_t *weights,
                                int16_t *history, size_t taps,
                                int16_t mu_q15, uint8_t normalized)
{
    if (!lms || !weights || !history || !taps || mu_q15 <= 0)
        return SIGQ15_EINVAL;
    lms->weights = weights;
    lms->history = history;
    lms->taps = taps;
    lms->index = 0;
    lms->mu_q15 = mu_q15;
    lms->normalized = normalized;
    memset(weights, 0, taps * sizeof(*weights));
    memset(history, 0, taps * sizeof(*history));
    return SIGQ15_OK;
}

void sigq15_lms_reset(sigq15_lms_t *lms)
{
    if (!lms) return;
    memset(lms->weights, 0, lms->taps * sizeof(*lms->weights));
    memset(lms->history, 0, lms->taps * sizeof(*lms->history));
    lms->index = 0;
}

int16_t sigq15_lms_process(sigq15_lms_t *lms, int16_t reference,
                           int16_t desired, int16_t *error,
                           sigq15_diag_t *diag)
{
    if (!lms) return 0;
    lms->history[lms->index] = reference;
    int64_t acc = 0;
    uint64_t energy = 1;
    size_t p = lms->index;
    for (size_t k = 0; k < lms->taps; ++k) {
        acc += (int32_t)lms->weights[k] * lms->history[p];
        energy += (uint64_t)((int32_t)lms->history[p] * lms->history[p]);
        p = p ? p - 1 : lms->taps - 1;
    }
    int16_t output = sigq15_sat((int32_t)(acc >> 15), diag);
    int16_t e = sigq15_sat((int32_t)desired - output, diag);
    int32_t mu = lms->mu_q15;
    if (lms->normalized) {
        uint64_t denominator = energy >> 15;
        if (denominator > 32768U)
            mu = (int32_t)(((int64_t)mu << 15) / denominator);
    }
    p = lms->index;
    for (size_t k = 0; k < lms->taps; ++k) {
        int32_t delta = (int32_t)(((int64_t)mu * e * lms->history[p]) >> 30);
        lms->weights[k] = sigq15_sat((int32_t)lms->weights[k] + delta, diag);
        p = p ? p - 1 : lms->taps - 1;
    }
    lms->index = (lms->index + 1) % lms->taps;
    if (error) *error = e;
    return output;
}

sigq15_status_t sigq31_fll_init(sigq31_fll_t *fll, uint32_t sample_rate_hz,
                                uint32_t initial, uint32_t minimum,
                                uint32_t maximum, int16_t gain)
{
    if (!fll || !sample_rate_hz || minimum >= maximum ||
        initial < minimum || initial > maximum || gain <= 0)
        return SIGQ15_EINVAL;
    fll->sample_rate_hz = sample_rate_hz;
    fll->frequency_millihz = initial;
    fll->center_millihz = initial;
    fll->min_millihz = minimum;
    fll->max_millihz = maximum;
    fll->gain_q15 = gain;
    return SIGQ15_OK;
}
uint32_t sigq31_fll_update(sigq31_fll_t *fll, int32_t phase_error_q32)
{
    if (!fll) return 0;
    int64_t correction = ((int64_t)phase_error_q32 * fll->gain_q15 *
                          fll->sample_rate_hz * 1000) >> 47;
    int64_t estimate = (int64_t)fll->center_millihz + correction;
    if (estimate < fll->min_millihz) estimate = fll->min_millihz;
    if (estimate > fll->max_millihz) estimate = fll->max_millihz;
    fll->frequency_millihz = (uint32_t)estimate;
    return fll->frequency_millihz;
}
sigq15_status_t sigq31_pll_init(sigq31_pll_t *pll, uint32_t sample_rate_hz,
                                uint32_t initial_millihz, int32_t kp_q30,
                                int32_t ki_q30, uint32_t min_millihz,
                                uint32_t max_millihz)
{
    if (!pll || !sample_rate_hz || min_millihz >= max_millihz ||
        initial_millihz < min_millihz || initial_millihz > max_millihz ||
        kp_q30 < 0 || ki_q30 < 0) return SIGQ15_EINVAL;
    memset(pll, 0, sizeof(*pll));
    pll->sample_rate_hz = sample_rate_hz;
    pll->frequency_millihz = initial_millihz;
    pll->kp_q30 = kp_q30;
    pll->ki_q30 = ki_q30;
    pll->min_millihz = min_millihz;
    pll->max_millihz = max_millihz;
    return SIGQ15_OK;
}

void sigq31_pll_reset(sigq31_pll_t *pll, uint32_t frequency_millihz)
{
    if (!pll || frequency_millihz < pll->min_millihz ||
        frequency_millihz > pll->max_millihz) return;
    pll->phase = 0;
    pll->frequency_millihz = frequency_millihz;
    pll->integrator_q30 = (int64_t)frequency_millihz << 30;
    pll->lock_metric = 0;
    pll->lock_count = 0;
}

int16_t sigq31_pll_process(sigq31_pll_t *pll, int16_t sample,
                           const int16_t *table, uint16_t table_size)
{
    if (!pll || !table || table_size < 4 || (table_size & (table_size - 1)))
        return 0;
    uint32_t index = (uint32_t)(((uint64_t)pll->phase * table_size) >> 32);
    int16_t cosine = table[(index + table_size / 4) & (table_size - 1)];
    int16_t error = (int16_t)(-(((int32_t)sample * cosine) >> 15));
    ++pll->samples_since_crossing;
    if (pll->previous_sample <= 0 && sample > 0 &&
        pll->samples_since_crossing > 1U) {
        uint32_t measured = (uint32_t)(((uint64_t)pll->sample_rate_hz * 1000U +
                                       pll->samples_since_crossing / 2U) /
                                      pll->samples_since_crossing);
        if (measured >= pll->min_millihz && measured <= pll->max_millihz) {
            uint32_t blended = (7U * pll->frequency_millihz + measured) / 8U;
            pll->integrator_q30 = (int64_t)blended << 30;
        }
        pll->samples_since_crossing = 0;
    }
    pll->previous_sample = sample;
    pll->integrator_q30 += (int64_t)pll->ki_q30 * error;
    int64_t frequency = (pll->integrator_q30 >> 30) +
                        (((int64_t)pll->kp_q30 * error) >> 30);
    if (frequency < pll->min_millihz) {
        frequency = pll->min_millihz;
        pll->integrator_q30 = frequency << 30;
    }
    if (frequency > pll->max_millihz) {
        frequency = pll->max_millihz;
        pll->integrator_q30 = frequency << 30;
    }    pll->frequency_millihz = (uint32_t)frequency;
    pll->phase += (uint32_t)(((uint64_t)pll->frequency_millihz << 32) /
                            ((uint64_t)pll->sample_rate_hz * 1000U));
    uint32_t absolute = error < 0 ? (uint32_t)-(int32_t)error : (uint32_t)error;
    pll->lock_metric = (pll->lock_metric * 255U + absolute) / 256U;
    pll->lock_count = pll->lock_metric < 1200 ? (uint16_t)(pll->lock_count + 1) : 0;
    return error;
}

uint8_t sigq31_pll_locked(const sigq31_pll_t *pll)
{
    return pll && pll->lock_count > 100;
}

static int32_t ratio_q20(int64_t numerator, uint64_t denominator)
{
    if (!denominator) return 0;
    uint8_t negative = numerator < 0;
    uint64_t absolute = negative ? (uint64_t)(-(numerator + 1)) + 1U :
                                   (uint64_t)numerator;
    uint64_t quotient = (absolute / denominator) << 20;
    uint64_t remainder = absolute % denominator;
    for (unsigned i = 0; i < 20; ++i) {
        uint64_t bit = (uint64_t)1 << (19U - i);
        if (remainder >= denominator - remainder) {
            remainder -= denominator - remainder;
            quotient |= bit;
        } else {
            remainder <<= 1;
        }
    }
    if (quotient > INT32_MAX) return negative ? INT32_MIN : INT32_MAX;
    return negative ? -(int32_t)quotient : (int32_t)quotient;
}

sigq15_status_t sigq15_transfer_point(const int16_t *input,
                                      const int16_t *output, size_t count,
                                      uint32_t sample_rate_hz,
                                      uint32_t frequency_hz,
                                      sigq15_transfer_result_t *result,
                                      sigq15_diag_t *diag)
{
    if (!input || !output || !result) return SIGQ15_EINVAL;
    sigq15_goertzel_result_t in, out;
    sigq15_status_t status = sigq15_goertzel(input, count, sample_rate_hz,
                                             frequency_hz, &in, diag);
    if (status != SIGQ15_OK) return status;
    status = sigq15_goertzel(output, count, sample_rate_hz,
                             frequency_hz, &out, diag);
    if (status != SIGQ15_OK) return status;
    int64_t ar = in.real_q15, ai = in.imag_q15;
    int64_t br = out.real_q15, bi = out.imag_q15;
    uint64_t denominator = (uint64_t)(ar * ar) + (uint64_t)(ai * ai);
    if (!denominator) return SIGQ15_ENOSIGNAL;
    result->real_q20 = ratio_q20(br * ar + bi * ai, denominator);
    result->imag_q20 = ratio_q20(bi * ar - br * ai, denominator);
    result->magnitude_q20 = (int32_t)isqrt64(
        (uint64_t)((int64_t)result->real_q20 * result->real_q20) +
        (uint64_t)((int64_t)result->imag_q20 * result->imag_q20));
    result->phase_q32 = atan2_q32(result->imag_q20, result->real_q20);
    result->coherence_q15 = 32767;
    result->valid = 1;
    return SIGQ15_OK;
}

static uint16_t ratio_q15_u64(uint64_t numerator, uint64_t denominator)
{
    if (!denominator) return 0;
    if (numerator >= denominator) return 32767;
    return (uint16_t)((numerator << 15) / denominator);
}
static int16_t db10_q8(uint64_t numerator, uint64_t denominator)
{
    if (!numerator) return INT16_MIN;
    if (!denominator) return INT16_MAX;
    int32_t exponent = 0;
    while (numerator >= denominator * 2U && denominator <= UINT64_MAX / 2U) {
        denominator <<= 1;
        ++exponent;
    }
    while (numerator < denominator && numerator <= UINT64_MAX / 2U) {
        numerator <<= 1;
        --exponent;
    }
    /* Normalize numerator/denominator to Q1.31 with bounded long division. */
    uint64_t remainder = numerator - denominator;
    uint64_t normalized = (uint64_t)1 << 31;
    for (int bit = 30; bit >= 0; --bit) {
        if (remainder >= denominator - remainder) {
            remainder -= denominator - remainder;
            normalized |= (uint64_t)1 << bit;
        } else {
            remainder <<= 1;
        }
    }
    uint32_t fraction = 0;
    for (unsigned i = 0; i < 16; ++i) {
        normalized = (normalized * normalized) >> 31;
        if (normalized >= ((uint64_t)2 << 31)) {
            normalized >>= 1;
            fraction |= (uint32_t)1 << (15U - i);
        }
    }
    int32_t log2_q16 = exponent * 65536 + (int32_t)fraction;
    int32_t db_q8 = (int32_t)(((int64_t)log2_q16 * 771) >> 16);
    if (db_q8 > INT16_MAX) return INT16_MAX;
    if (db_q8 < INT16_MIN) return INT16_MIN;
    return (int16_t)db_q8;
}
sigq15_status_t sigq15_spectrum_metrics(const uint32_t *power, size_t bins,
                                        size_t fundamental,
                                        size_t harmonic_count,
                                        sigq15_spectrum_metrics_t *result)
{
    if (!power || !result || bins < 2 || !fundamental ||
        fundamental >= bins || !power[fundamental]) return SIGQ15_EINVAL;
    uint64_t harmonic_power = 0, noise_power = 0, largest_spur = 0;
    for (size_t i = 1; i < bins; ++i) {
        if (i == fundamental) continue;
        uint8_t harmonic = 0;
        for (size_t h = 2; h <= harmonic_count; ++h)
            if (i == fundamental * h) { harmonic = 1; break; }
        if (harmonic) harmonic_power += power[i];
        else noise_power += power[i];
        if (power[i] > largest_spur) largest_spur = power[i];
    }
    uint32_t fundamental_rms = isqrt64(power[fundamental]);
    result->thd_q15 = ratio_q15_u64(isqrt64(harmonic_power), fundamental_rms);
    result->thdn_q15 = ratio_q15_u64(isqrt64(harmonic_power + noise_power),
                                     fundamental_rms);
    /* These compact fixed-point fields are linear power ratios; see manual. */
    result->snr_q8_db = db10_q8(power[fundamental], noise_power);
    result->sinad_q8_db = db10_q8(power[fundamental],
                                        harmonic_power + noise_power);
    result->sfdr_q8_db = db10_q8(power[fundamental], largest_spur);
    result->noise_power_q30 =
        noise_power > UINT32_MAX ? UINT32_MAX : (uint32_t)noise_power;
    result->valid = 1;
    return SIGQ15_OK;
}