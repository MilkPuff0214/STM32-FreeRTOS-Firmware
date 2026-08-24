#include "app_adc_task.h"

#include <stdint.h>
#include <stdio.h>

#include "FreeRTOS.h"
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

static void prvAdcTask(void *pvParameters)
{
    uint16_t usPotentiometerRaw;    //电位器原始ADC值
    uint16_t usTemperatureRaw;      //温度传感器原始ADC值
    char cReport[APP_ADC_REPORT_BUFFER_SIZE];
    int iReportLength;

    (void)pvParameters;

    for (;;)
    {
        /*
         * 启动后先阻塞1秒，让启动信息优先通过串口发送。
         * ADC Task绝大多数时间处于Blocked状态。
         */
        vTaskDelay(pdMS_TO_TICKS(APP_ADC_SAMPLE_PERIOD_MS));

        if (BspAdc_ReadRaw(
                &usPotentiometerRaw,
                &usTemperatureRaw) == false)
        {
            s_ulAdcReadFailureCount++;
            continue;
        }

        /*
         * 第一轮只输出原始ADC值，先验证采样链路。
         * 温度换算涉及芯片典型参数和个体误差，留到原始值验收后处理。
         */
        iReportLength = snprintf(
            cReport,
            sizeof(cReport),
            "ADC: potentiometer_raw=%u, temperature_raw=%u\r\n",
            (unsigned int)usPotentiometerRaw,
            (unsigned int)usTemperatureRaw);

        if ((iReportLength <= 0) ||
            (iReportLength >= (int)sizeof(cReport)))
        {
            s_ulAdcReportDropCount++;
            continue;
        }

        /*
         * AppSerial_Write会把内容复制进现有Serial TX Queue，
         * 因此ADC Task不需要等待DMA发送结束。
         */
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

    if (s_xAdcTaskHandle != NULL)
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
        s_xAdcTaskHandle = NULL;
        return false;
    }

    return true;
}