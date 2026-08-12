#include <stdint.h>
#include <stdio.h>

#include "sigq15_backends.h"

int main(void)
{
    int16_t input[256] = {0};
    int16_t packed[256] = {0};
    sigq15_backend_t backend = sigq15_backend_available();
    uint32_t root = sigq15_backend_sqrt_q30(1U << 30);
    if (sigq15_cmsis_available()) {
        if (sigq15_cmsis_rfft_q15(input, 256, packed) != SIGQ15_OK) return 1;
    }
    printf("backend=%d mathacl=%u sqrt_q30=%lu\n", (int)backend,
           sigq15_mathacl_available(), (unsigned long)root);
    return 0;
}
