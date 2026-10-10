#include "stm32f1xx.h"

/*
 * Minimal CMSIS system file for this project.
 * Reset_Handler calls SystemInit before main().
 * The application configures the 64 MHz clock in clock_init().
 */
uint32_t SystemCoreClock = 8000000U;

void SystemInit(void)
{
    /* Keep reset clock configuration; clock_init() configures PLL in main.c. */
}

void SystemCoreClockUpdate(void)
{
    /* The application uses a fixed 64 MHz system clock after clock_init(). */
    SystemCoreClock = 64000000U;
}
