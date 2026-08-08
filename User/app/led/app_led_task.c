#include "app_led_task.h"

#include "FreeRTOS.h"
#include "task.h"

#include "bsp_led.h"

void vLedTask(void *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        LED1_TOGGLE;
        vTaskDelay(pdMS_TO_TICKS(2 * 1000)); // —” ±2√Î
    }
}