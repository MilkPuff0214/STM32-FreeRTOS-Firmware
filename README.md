# STM32F103ZET6 固件工程

本目录承载野火 STM32F103 霸道 V2 的固件代码。当前先完成不依赖 FreeRTOS 的裸机 LED 冒烟测试，用它独立验证 ARM GCC 工具链、CMake + GNU Make 构建流程、GCC 启动文件、链接脚本、系统时钟和 GPIO。

当前仍处于项目 P0 环境验证阶段。裸机程序已经通过编译、链接和 ELF 静态检查，并已由用户在真实硬件上完成 CMSIS-DAP + OpenOCD 下载、PB5 红灯闪烁和 GDB 断点调试验证。下一步由用户以学习方式亲自移植 FreeRTOS Kernel V11.3.0；Codex 只提供讲解并维护本文档，不直接修改固件代码。CAN、W5500、MQTT、FTP 和 Bootloader 尚未接入。

## 当前技术栈

| 范围 | 当前选型 | 状态 |
| --- | --- | --- |
| 开发板 | 野火 STM32F103 霸道 V2 | 已确认 |
| MCU | STM32F103ZET6，Cortex-M3 | 已确认 |
| 板级库 | CMSIS + STM32F10x Standard Peripheral Library V3.5.0 | 已接入 |
| RTOS | FreeRTOS Kernel V11.3.0 | 已选型，准备下载和学习移植 |
| 交叉编译器 | GNU Tools for STM32 14.3.1，`arm-none-eabi-gcc` | 已验证 |
| 构建配置 | CMake 4.3.1 | 已验证 |
| 构建执行 | GNU Make 4.4.1 | 已验证 |
| 工具集合 | STM32CubeCLT 1.22.0 | 已安装 |
| 调试下载器 | CMSIS-DAP，SWD 接口 | 已完成实机下载验证 |
| 调试服务器 | OpenOCD | 已完成烧录和调试联调 |
| 源码调试器 | `arm-none-eabi-gdb` | 已完成断点调试验证 |

本工程不使用 Keil，也不使用 Ninja。CMake 不是编译器，它负责根据 `CMakeLists.txt` 生成 Makefile；GNU Make 执行 Makefile，并调用 `arm-none-eabi-gcc` 完成编译、汇编和链接。

```text
C/C/ASM 源文件
      ↓
CMakeLists.txt
      ↓ CMake 配置
build/Makefile
      ↓ GNU Make 执行
arm-none-eabi-gcc
      ↓
firmware.elf / firmware.hex / firmware.bin
```

## 当前目录结构

```text
firmware/
├── CMakeLists.txt
├── README.md
├── cmake/
│   └── arm-none-eabi-gcc.cmake       ARM GCC 交叉编译工具链文件
├── linker/
│   └── STM32F103ZETx_FLASH.ld        512 KiB Flash / 64 KiB SRAM 链接脚本
├── Libraries/
│   ├── CMSIS/
│   │   └── startup/gcc/
│   │       └── startup_stm32f103xe.S GCC 启动文件和中断向量表
│   ├── FreeRTOS-Kernel/               FreeRTOS V11.3.0 目标目录，当前为空
│   └── FWlib/                         STM32F10x 标准外设库
├── User/
│   ├── main.c                         当前裸机 LED 程序
│   └── led/                           霸道 V2 RGB LED BSP
└── build/                             构建输出，不纳入 Git
```

当前 CMake 目标只编译最小点灯所需文件。`User/` 中原有的按键、外部中断、串口和旧版 FreeRTOS 中断文件暂未加入构建，避免把未验证模块提前带入最小闭环。

## 硬件基线

- MCU：STM32F103ZET6，512 KiB 内部 Flash，64 KiB SRAM。
- 外部高速晶振：8 MHz；`SystemInit()` 将系统时钟配置为 72 MHz。
- 板载 RGB LED：共阳极，GPIO 输出低电平时点亮。
- 红灯：PB5；绿灯：PB0；蓝灯：PB1。
- 当前程序每 500 ms 翻转一次红灯。
- 上电测试前需要确认 RGB LED 供电跳帽 J73 已接通。
- 调试下载使用 CMSIS-DAP，通过开发板 SWD 接口连接。

## 构建环境检查

在 PowerShell 中确认以下命令均可执行：

```powershell
cmake --version
make --version
arm-none-eabi-gcc --version
arm-none-eabi-gdb --version
```

当前已验证的主要版本为：

```text
CMake                         4.3.1
GNU Make                      4.4.1
GNU Tools for STM32 / GCC    14.3.1
```

如果 PowerShell 提示无法识别 `make` 或 `arm-none-eabi-gcc`，应先检查 STM32CubeCLT 对应的 `bin` 目录是否已加入用户 `Path`，不要在 `CMakeLists.txt` 中写死本机绝对安装路径。

## 配置与构建

在 `firmware` 目录打开 PowerShell，第一次构建时执行：

```powershell
cmake -S . -B build -G "Unix Makefiles" "-DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake" "-DCMAKE_BUILD_TYPE=Debug"
cmake --build build -- -j8
```

PowerShell 中需要把两个 `-D...` 参数整体放在引号内，避免参数中的 `.cmake` 被错误拆分。

日常修改代码后，只需要增量构建：

```powershell
cmake --build build -- -j8
```

清理构建结果：

```powershell
cmake --build build --target clean
```

如果更换了交叉编译器或生成器，可以重新配置 CMake 缓存：

```powershell
cmake --fresh -S . -B build -G "Unix Makefiles" "-DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake" "-DCMAKE_BUILD_TYPE=Debug"
```

## 构建输出

构建成功后生成：

```text
build/firmware.elf    包含符号和调试信息，用于 GDB 调试
build/firmware.hex    Intel HEX 格式，可用于下载
build/firmware.bin    纯固件二进制，用于后续烧写和升级测试
build/firmware.map    链接映射，用于检查符号和内存占用
```

当前 Debug 版本的构建结果：

```text
FLASH：1268 B / 512 KiB
RAM：2568 B / 64 KiB
编译和链接警告：0
```

ELF 静态检查已经确认：

- ELF 类型为 32 位 ARM、little-endian、soft-float ABI。
- 中断向量表位于 `0x08000000`。
- 程序入口为 `Reset_Handler`。
- 主栈初始地址为 `0x20010000`，即 64 KiB SRAM 末端。
- Flash 段和 RAM 段的加载地址、大小和权限正确。

## 下载与实机验收

裸机 LED 基线已经由用户在真实硬件上完成以下验收：

- CMake + GNU Make + `arm-none-eabi-gcc` 配置、编译和链接成功。
- CMSIS-DAP 通过 SWD 与开发板正常连接。
- OpenOCD 成功下载并校验 `build/firmware.elf`。
- 复位后 PB5 红灯约每 500 ms 翻转一次。
- `arm-none-eabi-gdb` 可以连接目标并完成断点调试。

已验证的 OpenOCD 命令形式为：

```
openocd -f interface/cmsis-dap.cfg -c "transport select swd" -f target/stm32f1x.cfg -c "adapter speed 1000" -c "program build/firmware.elf verify reset exit"
```

以上结果说明启动文件、链接脚本、系统时钟、GPIO、构建、下载和基础调试链路已经形成最小闭环，可以把后续 FreeRTOS 问题与裸机板级问题分开定位。

## 固件技术路线

```text
P0  CMake + GNU Make + ARM GCC 裸机 LED 冒烟测试
 ↓
P1  FreeRTOS V11.3.0 + LED 任务 + vTaskDelay
 ↓
P1  bxCAN + Windows CAN 分析仪最小双向闭环
 ↓
P2  W5500 + TCP Socket + MQTT 3.1.1 + 业务心跳
 ↓
P3  FTP 流式下载 + W25Q64 暂存 + MD5 校验
 ↓
P4  Bootloader + 内部 Flash 更新 + 断电恢复
```

进入下一步 FreeRTOS LED Demo 时计划采用：

```text
FreeRTOS Kernel V11.3.0
portable/GCC/ARM_CM3
heap_4.c
FreeRTOSConfig.h
LED Task + vTaskDelay()
```

只有裸机点灯完成实机验证后，才加入 FreeRTOS。这样可以把启动文件、链接脚本、时钟和 GPIO 问题与调度器、SysTick、任务栈问题分开定位。

## FreeRTOS V11.3.0 学习型移植记录

本次移植由用户亲自完成，目标不仅是让 Demo 运行，还要理解每个内核文件、处理器端口、配置项和异常入口在系统中的作用。Codex 只负责讲解、协助分析用户提供的现象，以及根据真实进度维护本文档，不直接修改固件代码或构建文件。

### 1. 移植边界和完成标准

第一版只建立如下最小闭环：

```text
复位进入 main()
    ↓
初始化 LED GPIO
    ↓
创建一个 LED Task
    ↓
vTaskStartScheduler() 启动调度器
    ↓
SysTick 产生 RTOS Tick
    ↓
LED Task 使用 vTaskDelay() 周期阻塞和唤醒
    ↓
PB5 红灯约每 500 ms 翻转一次
```

本阶段不加入 CAN、W5500、MQTT、FTP、W25Q64 或 OTA 代码。最小 Demo 必须同时满足：构建和链接无警告、调度器不返回、LED 任务持续运行、错误 Hook 不触发、GDB 能观察任务以及 SysTick/PendSV/SVC 相关入口。

### 2. 总体步骤与当前状态

| 步骤 | 学习目标 | 当前状态 |
| --- | --- | --- |
| 0. 固化裸机基线 | 排除工具链、启动、链接、时钟、GPIO、下载器和 GDB 问题 | 已完成实机验证 |
| 1. 下载内核 | 从官方仓库取得并核验 FreeRTOS Kernel V11.3.0 | 目标目录已创建，源码尚未下载 |
| 2. 认识内核组成 | 理解通用内核、处理器端口、内存管理器和应用配置的边界 | 正在学习 |
| 3. 选择 Cortex-M3 端口 | 选择 `portable/GCC/ARM_CM3`，理解 SysTick、PendSV 和 SVC | 待执行 |
| 4. 选择堆实现 | 选择 `heap_4.c`，理解任务创建所需的动态内存 | 待执行 |
| 5. 创建应用配置 | 编写工程自己的 `FreeRTOSConfig.h` | 待执行 |
| 6. 接入 CMake | 只把最小 Demo 所需源文件和包含路径加入目标 | 待执行 |
| 7. 改造入口程序 | 删除裸机 SysTick 忙等，创建 LED Task 并启动调度器 | 待执行 |
| 8. 构建和静态检查 | 检查异常符号、内存占用、映射文件和编译警告 | 待执行 |
| 9. 烧录和 GDB 验证 | 验证调度器、Tick、上下文切换、任务阻塞和错误 Hook | 待执行 |
| 10. 最小 Demo 验收 | 连续运行并记录结果，形成可回退的 Git 基线 | 待执行 |

每完成一步，都应把实际命令、结果、遇到的问题和验证结论补充到本节，不能提前把计划写成已完成结果。

### 3. 官方源码下载和版本核验

只使用独立的官方内核仓库，不下载未经固定版本的 `main` 分支：

- 官方仓库：`https://github.com/FreeRTOS/FreeRTOS-Kernel`
- 指定版本源码：`https://github.com/FreeRTOS/FreeRTOS-Kernel/tree/V11.3.0`
- 指定版本发布页：`https://github.com/FreeRTOS/FreeRTOS-Kernel/releases/tag/V11.3.0`
- 官方发布对应的短提交号：`9b777ae`

当前采用 ZIP 快照方式，避免在独立的 `firmware` Git 仓库中意外创建嵌套仓库：

1. 从 V11.3.0 发布页的 Assets 下载 `Source code (zip)`。
2. 在工程外的临时目录解压。
3. 检查版本、许可证和必要文件。
4. 将解压后的目录改名为 `FreeRTOS-Kernel`。
5. 放到 `firmware/Libraries/FreeRTOS-Kernel/`，与 `CMSIS/`、`FWlib/` 并列。
6. 确认最终没有多套一层目录，也没有内嵌 `.git`。

正确路径示例：

```text
firmware/Libraries/FreeRTOS-Kernel/include/FreeRTOS.h
firmware/Libraries/FreeRTOS-Kernel/portable/GCC/ARM_CM3/port.c
firmware/Libraries/FreeRTOS-Kernel/portable/MemMang/heap_4.c
```

错误的多层路径示例：

```text
firmware/Libraries/FreeRTOS-Kernel/FreeRTOS-Kernel-11.3.0/include/FreeRTOS.h
```

下载后至少检查以下文件是否存在：

```text
include/FreeRTOS.h
include/task.h
portable/GCC/ARM_CM3/port.c
portable/GCC/ARM_CM3/portmacro.h
portable/MemMang/heap_4.c
tasks.c
list.c
LICENSE.md
History.txt
```

### 4. 内核顶层目录和源文件含义

官方压缩包的核心结构如下：

```text
FreeRTOS-Kernel-11.3.0/
├── include/
├── portable/
├── examples/
├── tasks.c
├── list.c
├── queue.c
├── timers.c
├── event_groups.c
├── stream_buffer.c
├── croutine.c
├── LICENSE.md
├── README.md
└── History.txt
```

各部分职责如下：

| 路径 | 作用 | 本次最小 Demo 如何处理 |
| --- | --- | --- |
| `include/` | FreeRTOS 的公共头文件和内核共享声明，例如 `FreeRTOS.h`、`task.h`、`queue.h`、`list.h` 和 `portable.h`。应用通过这些头文件使用 RTOS API | 整个目录加入编译器头文件搜索路径，不修改官方文件 |
| `portable/` | 把通用内核适配到不同 CPU、编译器和内存分配方案。这里是“移植”最核心的目录 | 只选择 GCC 的 Cortex-M3 端口和一个内存管理器，不编译其他架构端口 |
| `examples/` | 官方提供的配置模板、示例或测试参考，不属于运行时内核本体 | 只用于学习和参考，不直接加入固件目标 |
| `tasks.c` | 任务调度核心，包含任务创建、删除、延时、阻塞/就绪状态转换以及调度器启动等逻辑 | 最小 Demo 必须编译 |
| `list.c` | 内核内部双向链表实现。就绪列表、延时列表等调度数据结构依赖它 | 最小 Demo 必须编译；应用通常不直接调用它 |
| `queue.c` | 队列实现，也是信号量和互斥量的底层基础，因为这些对象复用了队列机制 | 纯 LED Task 暂时可不使用；引入队列或信号量时再加入 |
| `timers.c` | 软件定时器实现，并创建 Timer Service/Daemon Task 处理回调 | 第一版不用软件定时器，不加入 |
| `event_groups.c` | 事件组实现，用若干 bit 表示多个事件，适合一个任务等待多个条件 | 第一版不用，不加入 |
| `stream_buffer.c` | 字节流缓冲区和消息缓冲区实现，适合连续字节或可变长消息传递 | 第一版不用，不加入；后续串口/网络数据通道可再评估 |
| `croutine.c` | 旧式 Co-routine 实现，以更低资源代价在一个栈式环境中协作运行，不等同于普通 FreeRTOS Task | 本项目使用普通任务，不启用也不加入 |
| `LICENSE.md` | FreeRTOS Kernel 的开源许可证 | 必须保留并纳入 Git，不能删除 |
| `README.md` | 官方仓库说明、使用入口和集成提示 | 保留，作为上游资料 |
| `History.txt` | 内核版本历史和变更记录，可用于核对 V11.3.0 | 保留，用于版本追溯 |

`include/` 解决“应用和内核使用哪些声明”，顶层 `.c` 文件实现“与具体 MCU 无关的调度与同步机制”，`portable/` 解决“这些通用机制如何在当前 CPU、编译器和内存模型上运行”。三者共同组成一个可工作的 FreeRTOS 内核。

### 5. `portable/` 为什么是移植重点

FreeRTOS 的任务、队列和延时逻辑可以跨处理器复用，但保存寄存器、切换栈、启动第一个任务以及配置 Tick 定时器都与处理器和编译器有关。因此 `portable/` 通常按“编译器 + CPU 架构”组织。

本工程使用：

```text
编译器：arm-none-eabi-gcc
CPU：Cortex-M3
端口：portable/GCC/ARM_CM3
```

其中：

- `port.c` 实现 Cortex-M3 的调度器启动、SysTick 配置以及与上下文切换有关的底层逻辑。
- `portmacro.h` 定义栈类型、Tick 类型、临界区、任务切换和中断屏蔽等端口宏。
- Cortex-M3 通过 SVC 启动第一个任务，通过 PendSV 完成上下文切换，通过 SysTick 提供周期 Tick。
- 不能同时编译 ARM_CM3、ARM_CM4F、ARM_CM3_MPU 或其他端口，否则会出现架构不匹配或重复符号。

### 6. `portable/MemMang/` 和 `heap_4.c`

FreeRTOS 提供多种动态内存实现，应用一次只能选择一种：

| 文件 | 特点 | 是否适合本项目当前阶段 |
| --- | --- | --- |
| `heap_1.c` | 只分配、不释放，结构最简单且确定性强 | 可学习，但不适合作为后续长期工程基线 |
| `heap_2.c` | 支持释放，但不会合并相邻空闲块，长期运行可能碎片化 | 不选择 |
| `heap_3.c` | 封装 C 库 `malloc/free`，依赖 C 运行库及其线程安全行为 | 不选择 |
| `heap_4.c` | 支持分配和释放，并合并相邻空闲块，是常见单一 RAM 区域方案 | 本项目选择 |
| `heap_5.c` | 支持多个不连续内存区域，配置更复杂 | 当前 STM32F103 单 SRAM 最小 Demo 不需要 |

选择 `heap_4.c` 后，`FreeRTOSConfig.h` 中的 `configTOTAL_HEAP_SIZE` 决定 FreeRTOS 堆大小。任务控制块和动态创建的任务栈都从这个堆分配；它与链接脚本中的 C 运行库 Heap/主栈预留不是同一个概念，后续要结合 MAP 文件检查 64 KiB SRAM 的总占用。

### 7. 后续每一步要理解的内容

后续移植不直接一次性复制最终答案，而按以下顺序完成：

1. 下载并核验 V11.3.0，确认目录和版本来源。
2. 阅读 `FreeRTOS.h`、`task.h`、ARM_CM3 的 `port.c/portmacro.h`，建立通用内核与端口层概念。
3. 创建工程自己的 `FreeRTOSConfig.h`，逐项解释时钟、Tick、优先级、堆、栈和 Hook 配置。
4. 修改 CMake 源文件列表和包含路径，并解释每个加入项为什么必要。
5. 将裸机 `delay_ms()` 替换为任务和 `vTaskDelay()`，解释忙等与阻塞的区别。
6. 核对启动文件中的 SVC、PendSV、SysTick 弱符号如何被 FreeRTOS 端口接管。
7. 构建并检查 ELF、MAP、异常符号和 RAM/Flash 增量。
8. 烧录后用断点验证任务入口、SysTick、PendSV、错误 Hook 和调度器不返回。
9. 完成长时间运行验证，并将真实结果补充到本文档。

## 当前范围与限制

- 当前代码仅用于工具链和板级最小运行环境验证。
- 尚未接入 FreeRTOS，当前 `SysTick` 只用于裸机轮询延时。
- 尚未实现 CAN、W5500、TCP、MQTT、FTP、W25Q64 和 Bootloader。
- 不在固件源码或 README 中保存真实 MQTT/FTP 用户名、密码、证书私钥或设备唯一凭据。
- `build/` 中的 ELF、HEX、BIN、MAP 和中间文件不提交 Git。

## README 维护规则

后续每次代码更新，都要在同一个 Git 提交中同步检查并更新本文件。至少在以下内容变化时更新对应章节：

- 工具链、依赖版本或构建命令。
- 目录结构、源文件或 CMake 目标。
- 硬件引脚、时钟、存储空间或下载方式。
- 已完成功能、实机验证结果和当前限制。
- FreeRTOS、CAN、W5500、MQTT、FTP 或 Bootloader 的接入状态。
- 当前阶段和下一步技术路线。

影响整个系统架构或项目阶段的变化，还必须同步更新根目录 `README.md`、`docs/PROJECT_CONTEXT.md` 和 `docs/NEXT_STEPS.md`。
