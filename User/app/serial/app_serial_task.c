#include "app_serial_task.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "bsp_usart.h"

/* 最多缓存4条尚未发送的串口消息。 */
#define APP_SERIAL_TX_QUEUE_LENGTH    4U

#define APP_SERIAL_TX_STACK_DEPTH     configMINIMAL_STACK_SIZE
#define APP_SERIAL_TX_PRIORITY        (tskIDLE_PRIORITY + 1U)

typedef struct
{
    uint16_t usLength;
    uint8_t ucData[APP_SERIAL_TX_MESSAGE_MAX_LENGTH];
} AppSerialTxMessage_t;

/*
 * Queue和Task Handle都由Serial模块私有保存。
 * 其他模块只能通过AppSerial_Write()提交消息。
 */
static QueueHandle_t s_xSerialTxQueue = NULL;
static TaskHandle_t s_xSerialTxTaskHandle = NULL;

/*
 * 当前只有Serial TX Task修改该计数，不存在多Task写竞争。
 * 后续可以通过Console诊断命令读取它。
 */
static volatile uint32_t s_ulTxDmaStartFailureCount = 0U;


/*
 * 192 words在Cortex-M3上是768字节。
 * RX Task会调用AppSerial_Write()并产生额外调用栈，
 * 因此暂时比configMINIMAL_STACK_SIZE多保留一些空间。
 */
#define APP_SERIAL_RX_STACK_DEPTH    192U
#define APP_SERIAL_RX_PRIORITY       (tskIDLE_PRIORITY + 1U)

static TaskHandle_t s_xSerialRxTaskHandle = NULL;  //接收任务的任务句柄

/*
 * TX Queue满时，当前回显测试选择丢弃对应数据并累计次数。
 * 以后可通过诊断命令读取这个计数。
 */
static volatile uint32_t s_ulRxEchoDropCount = 0U;


static void prvSerialTxTask(void *pvParameters)
{
    AppSerialTxMessage_t xMessage;

    (void)pvParameters;

    for (;;)
    {
        /*
         * Queue为空时任务进入Blocked状态。
         * 新消息到达后，FreeRTOS把消息副本写入xMessage并唤醒任务。
         */
        if (xQueueReceive(
                s_xSerialTxQueue,
                &xMessage,
                portMAX_DELAY) == pdPASS)
        {
            /*
             * xMessage位于Serial TX Task自己的任务栈中。
             * Task不会在DMA完成前再次调用xQueueReceive()，
             * 所以DMA读取期间这段内存不会被覆盖。
             */
            if (BspUsart1_TxDmaStart(
                    xMessage.ucData,
                    xMessage.usLength) == true)
            {
                /*
                 * 等待DMA完成中断。
                 * ISR发送通知后，任务才能取出下一条Queue消息，
                 * 因此同一时刻只有一块TX缓冲区交给DMA。
                 */
                (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            }
            else
            {
                /*
                 * Serial TX Task是唯一DMA启动者，正常情况下不应失败。
                 * 当前先记录异常并丢弃本条消息，后续再增加恢复策略。
                 */
                s_ulTxDmaStartFailureCount++;
            }
        }
    }
}

bool AppSerialTxTask_Create(void)
{
    BaseType_t xCreateResult;

    /*
     * 当前模块只能创建一次，防止出现两个Task同时拥有USART TX。
     */
    if (s_xSerialTxQueue != NULL)
    {
        return false;
    }

    s_xSerialTxQueue = xQueueCreate(
        APP_SERIAL_TX_QUEUE_LENGTH,
        sizeof(AppSerialTxMessage_t));

    if (s_xSerialTxQueue == NULL)
    {
        return false;
    }

    xCreateResult = xTaskCreate(
        prvSerialTxTask,
        "SERIAL_TX",
        APP_SERIAL_TX_STACK_DEPTH,
        NULL,
        APP_SERIAL_TX_PRIORITY,
        &s_xSerialTxTaskHandle);

    if (xCreateResult != pdPASS)
    {
        /*
         * Task创建失败时释放已经创建的Queue，
         * 避免模块处于“有Queue但没有消费者”的半初始化状态。
         */
        vQueueDelete(s_xSerialTxQueue);
        s_xSerialTxQueue = NULL;
        s_xSerialTxTaskHandle = NULL;
        return false;
    }

    return true;
}

bool AppSerial_Write(const uint8_t *pData, uint16_t length)
{
    AppSerialTxMessage_t xMessage = {0};

    if ((pData == NULL) ||
        (length == 0U) ||
        (length > APP_SERIAL_TX_MESSAGE_MAX_LENGTH) ||
        (s_xSerialTxQueue == NULL))
    {
        return false;
    }

    xMessage.usLength = length;
    memcpy(xMessage.ucData, pData, length);

    /*
     * 使用0 Tick表示Queue满时立即返回，不阻塞调用者。
     * 日志系统不能因为串口速度较慢而无限阻塞控制任务。
     */
    return xQueueSend(
               s_xSerialTxQueue,
               &xMessage,
               0U) == pdPASS;
}

void DMA1_Channel4_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken;

    xHigherPriorityTaskWoken = pdFALSE;

    if (BspUsart1_TxDmaHandleInterrupt() == true)
    {
        if (s_xSerialTxTaskHandle != NULL)
        {
            vTaskNotifyGiveFromISR(
                s_xSerialTxTaskHandle,
                &xHigherPriorityTaskWoken);  
        }
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static void prvSerialRxTask(void *pvParameters)
{
    const uint8_t *pucRxBuffer;
    uint16_t usReadPosition;
    uint16_t usWritePosition;
    uint16_t usChunkLength; //本次处理的数据块长度

    (void)pvParameters;

    pucRxBuffer = BspUsart1_RxDmaGetBuffer();
    usReadPosition = 0U;

    /*
     * 当前已经处于Task上下文，调度器也已经运行，
     * 此时才启动RX DMA和允许USART1 IDLE中断。
     */
    BspUsart1_RxDmaStart();

    for (;;)
    {
        /*
         * 没有IDLE事件时进入Blocked状态。
         * ISR只通知Task，不在中断中复制或回显数据。
         */
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        /*
         * 只处理取得写位置快照时已经收到的数据。
         * 快照之后到达的新数据留到下一次IDLE事件处理。
         */
        usWritePosition = BspUsart1_RxDmaGetWritePosition();

        while (usReadPosition != usWritePosition)
        {
            if (usWritePosition > usReadPosition)
            {
                usChunkLength =
                    usWritePosition - usReadPosition;
            }
            else
            {
                /*
                 * DMA已经回绕，先处理缓冲区尾部，
                 * 处理到256后再从位置0继续。
                 */
                usChunkLength =
                    BSP_USART1_RX_DMA_BUFFER_SIZE -
                    usReadPosition;
            }

            /*
             * AppSerial_Write()单条消息最多接受128字节，
             * 因此较长数据必须拆成多个Queue消息。
             */
            if (usChunkLength >
                APP_SERIAL_TX_MESSAGE_MAX_LENGTH)
            {
                usChunkLength =
                    APP_SERIAL_TX_MESSAGE_MAX_LENGTH;
            }

            if (AppSerial_Write(
                    &pucRxBuffer[usReadPosition],
                    usChunkLength) == false)
            {
                /*
                 * 当前回显实验不允许RX Task无限等待TX Queue。
                 * Queue满时记录丢弃，并继续推进读取位置。
                 */
                s_ulRxEchoDropCount++;
            }

            usReadPosition += usChunkLength;

            if (usReadPosition >=
                BSP_USART1_RX_DMA_BUFFER_SIZE)
            {
                usReadPosition = 0U;
            }
        }
    }
}


bool AppSerialRxTask_Create(void)
{
    BaseType_t xCreateResult;

    /*
     * RX回显依赖现有TX Queue，而且RX Task只允许创建一次。
     */
    if ((s_xSerialTxQueue == NULL) ||
        (s_xSerialRxTaskHandle != NULL))
    {
        return false;
    }

    xCreateResult = xTaskCreate(
        prvSerialRxTask,
        "SERIAL_RX",
        APP_SERIAL_RX_STACK_DEPTH,
        NULL,
        APP_SERIAL_RX_PRIORITY,
        &s_xSerialRxTaskHandle);

    if (xCreateResult != pdPASS)
    {
        s_xSerialRxTaskHandle = NULL;
        return false;
    }

    return true;
}

void USART1_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken;

    xHigherPriorityTaskWoken = pdFALSE;

    if (BspUsart1_RxIdleHandleInterrupt() == true)
    {
        if (s_xSerialRxTaskHandle != NULL)
        {
            vTaskNotifyGiveFromISR(
                s_xSerialRxTaskHandle,
                &xHigherPriorityTaskWoken);
        }
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}