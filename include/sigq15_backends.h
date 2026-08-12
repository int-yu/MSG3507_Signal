#ifndef SIGQ15_BACKENDS_H
#define SIGQ15_BACKENDS_H
#include "sigq15.h"
#ifdef __cplusplus
extern "C" {
#endif
uint8_t sigq15_cmsis_available(void);
sigq15_status_t sigq15_cmsis_rfft_q15(int16_t *input, uint16_t length,
                                      int16_t *packed_output);
uint8_t sigq15_mathacl_available(void);
uint32_t sigq15_backend_sqrt_q30(uint32_t value_q30);
#ifdef __cplusplus
}
#endif
#endif