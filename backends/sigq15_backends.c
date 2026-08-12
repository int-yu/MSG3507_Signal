#include "sigq15_backends.h"
#ifdef SIGQ15_USE_CMSIS_DSP
#include "arm_math.h"
uint8_t sigq15_cmsis_available(void) { return 1; }
sigq15_status_t sigq15_cmsis_rfft_q15(int16_t *input, uint16_t length,
                                      int16_t *packed_output)
{
    if (!input || !packed_output || (length != 256U && length != 512U))
        return SIGQ15_EINVAL;
    arm_rfft_instance_q15 instance;
    if (arm_rfft_init_q15(&instance, length, 0, 1) != ARM_MATH_SUCCESS)
        return SIGQ15_ERANGE;
    arm_rfft_q15(&instance, input, packed_output);
    return SIGQ15_OK;
}
#else
uint8_t sigq15_cmsis_available(void) { return 0; }
sigq15_status_t sigq15_cmsis_rfft_q15(int16_t *input, uint16_t length,
                                      int16_t *packed_output)
{
    (void)input; (void)length; (void)packed_output;
    return SIGQ15_ERANGE;
}
#endif
#ifdef SIGQ15_USE_MATHACL
#include <ti/driverlib/dl_mathacl.h>
uint8_t sigq15_mathacl_available(void) { return 1; }
uint32_t sigq15_backend_sqrt_q30(uint32_t value_q30)
{
    /*
     * Deliberately conservative adapter: SDK-specific register sequencing must
     * be benchmarked on the target board before replacing this integer result.
     */
    uint32_t result = 0, bit = 1UL << 30;
    while (bit > value_q30) bit >>= 2;
    while (bit) {
        if (value_q30 >= result + bit) {
            value_q30 -= result + bit;
            result = (result >> 1) + bit;
        } else result >>= 1;
        bit >>= 2;
    }
    return result;
}
#else
uint8_t sigq15_mathacl_available(void) { return 0; }
uint32_t sigq15_backend_sqrt_q30(uint32_t value_q30)
{
    uint32_t result = 0, bit = 1UL << 30;
    while (bit > value_q30) bit >>= 2;
    while (bit) {
        if (value_q30 >= result + bit) {
            value_q30 -= result + bit;
            result = (result >> 1) + bit;
        } else result >>= 1;
        bit >>= 2;
    }
    return result;
}
#endif
sigq15_backend_t sigq15_backend_available(void)
{
#ifdef SIGQ15_USE_CMSIS_DSP
    return SIGQ15_BACKEND_CMSIS;
#elif defined(SIGQ15_USE_MATHACL)
    return SIGQ15_BACKEND_MATHACL;
#else
    return SIGQ15_BACKEND_PORTABLE;
#endif
}