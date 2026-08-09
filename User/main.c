#include "FreeRTOS.h"
#include "task.h"

#include "bsp_led.h"
#include "app_led_task.h"
#include "bsp_key.h"
#include "app_key_task.h"
#include "app_key_event.h"

#include "queue.h"
/*
 * 队列最多缓存4个尚未处理的按键事件。
 * 这是队列项目数量，不是字节数量，也不是按键种类数量。
 */
#define APP_KEY_EVENT_QUEUE_LENGTH 4U

int main(void)
{
    BaseType_t xTaskCreateResult;
    QueueHandle_t xKeyEventQueue;
    /* 调度器启动前完成所有基础硬件初始化。 */
    LED_GPIO_Config();
    BspKey_Init();

    /*
    * Queue中的每一个项目都是一个AppKeyEvent_t。
    * FreeRTOS会为Queue控制结构和4个事件的存储空间分配内存。
uxQueueLength
= 队列容量
= 最多能放几个消息/元素

uxItemSize
= 每个消息/元素的大小
= 单位：Byte
    */
    xKeyEventQueue = xQueueCreate(
        APP_KEY_EVENT_QUEUE_LENGTH,
        sizeof(AppKeyEvent_t));
    
    if (xKeyEventQueue == NULL)
    {
        /*
        * 返回NULL通常表示FreeRTOS Heap不足。
        * 调度器尚未启动，因此停在这里保留故障现场。
        */
        for (;;)
        {
        }
    }
    /* 创建LED任务 */
    xTaskCreateResult = xTaskCreate(
        vLedTask, 
        "LED", 
        configMINIMAL_STACK_SIZE, 
        xKeyEventQueue, 
        tskIDLE_PRIORITY + 1, 
        NULL);

    if (xTaskCreateResult != pdPASS)
    {
        for (;;)
        {
        }
    }

    /* Key Task只负责周期扫描和消抖，不进行阻塞式按键等待。 */
    xTaskCreateResult = xTaskCreate(
        vKeyTask,
        "KEY",
        configMINIMAL_STACK_SIZE,
        xKeyEventQueue,
        tskIDLE_PRIORITY + 1U,
        NULL);

    if (xTaskCreateResult != pdPASS)
    {
        /*
        * 调度器尚未启动，任务创建失败后停在这里，
        * 便于通过GDB检查FreeRTOS Heap。
        */
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