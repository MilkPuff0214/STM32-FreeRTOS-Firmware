#ifndef APP_ADC_TASK_H
#define APP_ADC_TASK_H

#include <stdbool.h>

/**
 * @brief 创建ADC周期采样任务。
 * @return true表示创建成功，false表示任务已创建或创建失败。
 */
bool AppAdcTask_Create(void);

#endif