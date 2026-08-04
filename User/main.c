#include <stdint.h>

#include "bsp_led.h"

// 72 MHz的含义是每秒 72,000,000 个时钟周期
static void delay_ms(uint32_t milliseconds)
{
    const uint32_t ticks_per_millisecond = SystemCoreClock / 1000U;

    if ((ticks_per_millisecond == 0U) || (ticks_per_millisecond > 0x01000000U))
    {
        while (1)
        {
        }
    }

    SysTick->LOAD = ticks_per_millisecond - 1U;
    SysTick->VAL = 0U;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;   //使用使用处理器时钟作为计数时钟

    for (uint32_t elapsed = 0U; elapsed < milliseconds; ++elapsed)
    {
        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0U)
        {
        }
    }

    SysTick->CTRL = 0U;
    SysTick->VAL = 0U;
}

int main(void)
{
    LED_GPIO_Config();

    while (1)
    {
        LED1_TOGGLE;
        delay_ms(500U);
    }
}
