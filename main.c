/*
 * MSPM0G3507 Signal - hardware-neutral SysConfig shell.
 * The algorithm library never configures pins or peripherals.
 */
#include "ti_msp_dl_config.h"
#include "sigq15.h"

int main(void)
{
    SYSCFG_DL_init();
    /* Connect DMA ping/pong buffers to sigq15_* calls in the board layer. */
    for (;;) __WFI();
}