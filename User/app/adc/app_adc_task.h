#ifndef APP_ADC_TASK_H
#define APP_ADC_TASK_H

#include <stdbool.h>
#include <stdint.h>

/*
 * 这是ADC Task向其他应用模块发布的完整样本快照。
 * 有效标志和对应原始值必须作为同一个Queue元素一起复制，
 * 避免消费者读到一半旧数据、一半新数据。
 */
typedef struct
{
    uint16_t usPotentiometerRaw;
    uint16_t usTemperatureRaw;
    bool xPotentiometerValid;
    bool xTemperatureValid;
} AppAdcSample_t;

/**
 * @brief 创建ADC周期采样任务及最新样本Queue。
 * @return true表示创建成功，false表示已经创建或创建失败。
 */
bool AppAdcTask_Create(void);

/**
 * @brief 非阻塞读取最新ADC样本的完整副本。
 * @param pxSample 用于接收样本副本。
 * @return true表示已有样本，false表示尚未发布样本或参数无效。
 */
bool AppAdc_TryReadLatestSample(AppAdcSample_t *pxSample);

#endif