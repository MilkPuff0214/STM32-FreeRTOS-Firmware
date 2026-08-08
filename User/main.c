#include "FreeRTOS.h"
#include "task.h"

#include "bsp_led.h"
#include "app_led_task.h"

int main(void)
{
    BaseType_t xTaskCreateResult;
    /* 初始化LED的GPIO */
    LED_GPIO_Config();

    /* 创建LED任务 */
    xTaskCreateResult = xTaskCreate(
        vLedTask, 
        "LED", 
        configMINIMAL_STACK_SIZE, 
        NULL, 
        tskIDLE_PRIORITY + 1, 
        NULL);

    if (xTaskCreateResult != pdPASS)
    {
        for (;;)
        {
        }
    }
    /* 启动调度器 */
    vTaskStartScheduler();

    /* 如果调度器启动失败，程序会运行到这里 */
    for (;;)
    {
        /* 可以在这里添加错误处理代码 */
    }
}