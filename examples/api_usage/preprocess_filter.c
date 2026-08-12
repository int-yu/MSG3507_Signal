#include <stdint.h>
#include <stdio.h>

#include "sigq15.h"

static int16_t adc_to_q15(uint16_t adc_code)
{
    return (int16_t)((int32_t)adc_code - 2048) << 4;
}

int main(void)
{
    static const int16_t fir_coeffs[3] = {8192, 16384, 8192};
    static const sigq15_biquad_coeffs_t sos_coeffs[1] = {
        {16384, 0, 0, 0, 0, 0}
    };
    static int16_t average_state[8];
    static int16_t fir_state[3];
    static sigq15_biquad_state_t sos_state[1];
    static const uint16_t adc_frame[16] = {
        2048, 2050, 2051, 2047, 2052, 2050, 2049, 2048,
        2047, 2048, 2050, 2051, 2049, 2048, 2047, 2048
    };
    int16_t decimated[8];
    sigq15_diag_t diag = {0};
    sigq15_dc_blocker_t dc;
    sigq15_moving_average_t average;
    sigq15_fir_t fir;
    sigq15_decimator_t decimator;
    sigq15_biquad_t sos;

    if (sigq15_dc_blocker_init(&dc, 32600) != SIGQ15_OK ||
        sigq15_moving_average_init(&average, average_state, 8, 8) != SIGQ15_OK ||
        sigq15_fir_init(&fir, fir_coeffs, 3, fir_state, 3) != SIGQ15_OK ||
        sigq15_decimator_init(&decimator, &fir, 2) != SIGQ15_OK ||
        sigq15_biquad_init(&sos, sos_coeffs, 1, sos_state, 1) != SIGQ15_OK)
        return 1;

    for (uint16_t i = 0; i < 16; ++i) {
        int16_t x = adc_to_q15(adc_frame[i]);
        x = sigq15_linear_calibrate(x, 32767, 0, &diag);
        x = sigq15_dc_blocker_process(&dc, x, &diag);
        x = sigq15_moving_average_process(&average, x, &diag);
        x = sigq15_biquad_process(&sos, x, &diag);
        (void)sigq15_decimator_process(&decimator, &x, 1, decimated,
                                       sizeof(decimated) / sizeof(decimated[0]), &diag);
    }
    sigq15_dc_blocker_reset(&dc);
    sigq15_moving_average_reset(&average);
    sigq15_decimator_reset(&decimator);
    printf("saturation=%lu valid=%u\n", (unsigned long)diag.saturation_count, diag.valid);
    return 0;
}
