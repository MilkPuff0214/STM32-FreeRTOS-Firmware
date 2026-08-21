#ifndef APP_CONSOLE_H
#define APP_CONSOLE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    /*
     * 指向Console模块内部保存的只读响应。
     * 调用者不能修改或释放这段内存。
     */
    const uint8_t *pucData;
    uint16_t usLength;
} AppConsoleOutput_t;

/*
 * 重置Console行缓冲区和协议状态。
 * 仅由Serial RX Task在开始接收前调用一次。
 */
void AppConsole_Init(void);

/*
 * 向Console状态机提交一个接收字节。
 *
 * 返回true表示已经产生一条响应，此时pOutput有效；
 * 返回false表示一行尚未完成，或者当前无需输出。
 *
 * 输出指针由Console模块持有，调用者应立即使用
 * AppSerial_Write()复制数据，不能长期保存该指针。
 *
 * 本接口只允许Serial RX Task调用，
 * 不能由多个Task并发调用，也不能在ISR中调用。
 */
bool AppConsole_ProcessByte(
    uint8_t ucByte,
    AppConsoleOutput_t *pOutput);

#endif /* APP_CONSOLE_H */