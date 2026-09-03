#include "app_adc_task.h"

#include <stdint.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "app_serial_task.h"
#include "bsp_adc.h"

#define APP_ADC_SAMPLE_PERIOD_MS       1000U
#define APP_ADC_TASK_STACK_DEPTH       256U
#define APP_ADC_TASK_PRIORITY          (tskIDLE_PRIORITY + 1U)
#define APP_ADC_REPORT_BUFFER_SIZE     96U

static TaskHandle_t s_xAdcTaskHandle = NULL;    //任务句柄

/* 便于调试器观察ADC读取失败和串口报告丢弃次数。 */
static volatile uint32_t s_ulAdcReadFailureCount = 0U;
static volatile uint32_t s_ulAdcReportDropCount = 0U;


#define APP_ADC_SAMPLE_QUEUE_LENGTH    1U

static QueueHandle_t s_xAdcSampleQueue = NULL;  //ADC样本队列句柄

/*
 * 长度为1的Queue使用xQueueOverwrite发布最新值。
 * 正常情况下overwrite不会因为Queue已满而失败。
 */
static volatile uint32_t s_ulAdcSamplePublishFailureCount = 0U;

static void prvAdcTask(void *pvParameters)
{
    AppAdcSample_t xSample;
    char cReport[APP_ADC_REPORT_BUFFER_SIZE];
    int iReportLength;

    (void)pvParameters;

    for (;;)
    {
        /*
         * 启动后先阻塞1秒，让启动信息优先通过串口发送。
         * ADC Task是ADC1的唯一所有者。
         */
        vTaskDelay(pdMS_TO_TICKS(APP_ADC_SAMPLE_PERIOD_MS));

        /*
         * 每轮先清空整个样本。
         * 如果本次读取失败，有效标志保持false。
         */
        xSample = (AppAdcSample_t){0};

        if (BspAdc_ReadRaw(
                &xSample.usPotentiometerRaw,
                &xSample.usTemperatureRaw) == true)
        {
            xSample.xPotentiometerValid = true;
            xSample.xTemperatureValid = true;
        }
        else
        {
            s_ulAdcReadFailureCount++;
        }

        /*
         * Queue长度必须为1。
         * 新样本直接覆盖旧样本，不积压已经失去实时价值的数据。
         *
         * 读取失败时也发布无效样本，防止CAN Task永远把上一次
         * 成功采样误认为当前仍然有效。
         */
        if (xQueueOverwrite(
                s_xAdcSampleQueue,
                &xSample) != pdPASS)
        {
            s_ulAdcSamplePublishFailureCount++;
        }

        /*
         * 串口只打印成功取得的完整样本。
         * Queue发布放在snprintf和串口发送之前，避免串口路径
         * 延迟CAN Task取得最新样本。
         */
        if ((xSample.xPotentiometerValid == false) ||
            (xSample.xTemperatureValid == false))
        {
            continue;
        }

        iReportLength = snprintf(
            cReport,
            sizeof(cReport),
            "ADC: potentiometer_raw=%u, temperature_raw=%u\r\n",
            (unsigned int)xSample.usPotentiometerRaw,
            (unsigned int)xSample.usTemperatureRaw);

        if ((iReportLength <= 0) ||
            (iReportLength >= (int)sizeof(cReport)))
        {
            s_ulAdcReportDropCount++;
            continue;
        }

        if (AppSerial_Write(
                (const uint8_t *)cReport,
                (uint16_t)iReportLength) == false)
        {
            s_ulAdcReportDropCount++;
        }
    }
}

bool AppAdcTask_Create(void)
{
    BaseType_t xCreateResult;

    if ((s_xAdcTaskHandle != NULL) ||
        (s_xAdcSampleQueue != NULL))
    {
        return false;
    }

    s_xAdcSampleQueue = xQueueCreate(
        APP_ADC_SAMPLE_QUEUE_LENGTH,
        sizeof(AppAdcSample_t));

    if (s_xAdcSampleQueue == NULL)
    {
        return false;
    }

    xCreateResult = xTaskCreate(
        prvAdcTask,
        "ADC",
        APP_ADC_TASK_STACK_DEPTH,
        NULL,
        APP_ADC_TASK_PRIORITY,
        &s_xAdcTaskHandle);

    if (xCreateResult != pdPASS)
    {
        vQueueDelete(s_xAdcSampleQueue);
        s_xAdcSampleQueue = NULL;
        s_xAdcTaskHandle = NULL;
        return false;
    }

    return true;
}

bool AppAdc_TryReadLatestSample(AppAdcSample_t *pxSample)
{
    if ((pxSample == NULL) ||
        (s_xAdcSampleQueue == NULL))
    {
        return false;
    }

    /*
     * 使用Peek而不是Receive：
     * Peek复制最新样本但不把它从Queue删除，因此在下一次ADC采样
     * 到来之前，CAN Task仍然可以取得当前最新的完整样本。
     *
     * 等待时间为0，CAN Task绝不会在这里等待ADC Task。
     */
    return xQueuePeek(
               s_xAdcSampleQueue,
               pxSample,
               0U) == pdPASS;
}