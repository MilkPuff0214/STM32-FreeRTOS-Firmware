# STM32F103ZET6 固件工程

本目录承载野火 STM32F103 霸道 V2 的固件代码。工程已经先用不依赖 FreeRTOS 的裸机 LED 冒烟测试验证 ARM GCC 工具链、CMake + GNU Make 构建流程、GCC 启动文件、链接脚本、系统时钟和 GPIO；当前正在由用户亲自完成 FreeRTOS Kernel V11.3.0 最小 LED Task 的移植与调试。

FreeRTOS 的原理、术语、源码关系、自测题和逐步移植学习记录见 [FreeRTOS 学习与移植笔记](FreeRTOS学习.md)。本 README 继续作为固件工程状态、构建方法和验收结果的入口。

裸机程序已经通过编译、链接和 ELF 静态检查，并已由用户在真实硬件上完成 CMSIS-DAP + OpenOCD 下载、PB5 红灯闪烁和 GDB 断点调试验证。FreeRTOS 最小 Demo 已完成源码接入、Task 创建、调度器启动路径构建和 ELF 静态检查；当前等待重新构建最新注释后进行 FreeRTOS Demo 的 OpenOCD 烧录与 GDB 动态验证。Codex 只提供讲解、只读检查并维护 Markdown，不直接修改固件代码。CAN、W5500、MQTT、FTP 和 Bootloader 尚未接入。

## 目录

- [当前技术栈](#当前技术栈)
- [固件目录与分层目标](#固件目录与分层目标)
- [项目自编 C 代码风格](#项目自编-c-代码风格)
- [硬件基线](#硬件基线)
- [构建环境检查](#构建环境检查)
- [配置与构建](#配置与构建)
- [构建输出](#构建输出)
- [下载与实机验收](#下载与实机验收)
- [固件技术路线](#固件技术路线)
- [FreeRTOS V11.3.0 学习型移植记录](#freertos-v1130-学习型移植记录)
- [当前范围与限制](#当前范围与限制)
- [README 维护规则](#readme-维护规则)

## 当前技术栈

| 范围 | 当前选型 | 状态 |
| --- | --- | --- |
| 开发板 | 野火 STM32F103 霸道 V2 | 已确认 |
| MCU | STM32F103ZET6，Cortex-M3 | 已确认 |
| 板级库 | CMSIS + STM32F10x Standard Peripheral Library V3.5.0 | 已接入 |
| RTOS | FreeRTOS Kernel V11.3.0 | 首个LED Task和调度器启动路径已构建并通过ELF静态检查；最新DWARF已同步，`main()`和`xTaskCreate()`已命中，六个Task创建实参与配置一致 |
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

## 固件目录与分层目标

用户已经把LED BSP从 `User/led/` 迁移到 `User/bsp/led/`，并同步更新CMake源文件和包含路径。迁移后的正式ELF尺寸与迁移前一致，`GNU_STACK`仍为 `RW`，说明这次纯目录重构没有改变当前固件内容。下面是当前已经采用并供后续模块遵循的分层结构：

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
│   ├── FreeRTOS-Kernel/               FreeRTOS V11.3.0 最小内核依赖
│   │   ├── include/                   内核公共头文件
│   │   ├── portable/GCC/ARM_CM3/      GNU GCC Cortex-M3 端口
│   │   ├── portable/MemMang/heap_4.c  动态内存管理实现
│   │   ├── tasks.c                    任务和调度器核心
│   │   └── list.c                     内核链表实现
│   └── FWlib/                         STM32F10x 标准外设库
├── User/
│   ├── main.c                         程序入口；后续只保留初始化、创建任务和启动调度器
│   ├── FreeRTOSConfig.h               当前工程的FreeRTOS编译配置
│   ├── rtos/                          应用与FreeRTOS内核之间的集成层
│   │   └── freertos_hooks.c           Malloc/栈溢出Hook，已由用户创建并通过语法检查
│   ├── app/                           应用Task和业务编排，按功能域继续分子目录
│   │   └── led/                       LED应用任务：app_led_task.c/.h已编写并加入CMake
│   └── bsp/                           板级支持层，按外设或器件继续分子目录
│       └── led/                       霸道 V2 RGB LED BSP：bsp_led.c/.h
└── build/                             构建输出，不纳入 Git
```

当前 CMake 目标从 `User/bsp/led/` 编译裸机LED BSP，同时编译FreeRTOS最小依赖：`tasks.c`、`list.c`、GNU Cortex-M3 `port.c`、`heap_4.c` 和应用Hook。Queue、Timer、Event Group、Stream Buffer、按键、外部中断、串口和其他未验证模块仍未加入构建，避免提前扩大最小闭环。

新增固件代码按职责落位，不能仅为省事堆在 `User/` 根目录：`Libraries/` 保存第三方内核和厂商库；`User/rtos/` 保存FreeRTOS应用侧Hook与适配；`User/app/<功能域>/` 保存Task入口和业务编排；`User/bsp/<外设或器件>/` 保存板级驱动。`main.c` 和全工程配置头可保留为根级入口，但不得逐步演变成业务实现集合。依赖方向应保持为：`main` 只调用必要的BSP初始化、应用模块注册和调度器启动，`app → bsp + FreeRTOS API`，`bsp → FWlib/CMSIS/硬件`；底层不得反向依赖应用层。

## 项目自编 C 代码风格

`firmware/User/` 及后续项目自编C代码统一采用：函数圆括号内部不留空格、控制关键字后留一个空格、强制转换写作 `(void)x`、无限循环写作 `for (;;)`、指针星号靠变量名，以及Allman大括号。Codex后续提供的项目代码示例也必须遵守该格式。

本规则不用于批量改写 `Libraries/` 中的FreeRTOS、CMSIS和FWlib第三方源码；引用上游源码讲解时可以保留其原始格式。完整强制规则见根目录 `AGENTS.md`，学习说明见 `FreeRTOS学习.md`。

## 硬件基线

- MCU：STM32F103ZET6，512 KiB 内部 Flash，64 KiB SRAM。
- 外部高速晶振：8 MHz；`SystemInit()` 将系统时钟配置为 72 MHz。
- 板载 RGB LED：共阳极，GPIO 输出低电平时点亮。
- 红灯：PB5；绿灯：PB0；蓝灯：PB1。
- 裸机基线程序每 500 ms 翻转一次红灯；当前FreeRTOS Demo源码改为每 2000 ms 翻转一次，因此完整亮灭周期约为 4 s。
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

裸机基线 Debug 版本的构建结果：

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

### FreeRTOS最小源码接入构建结果

用户把五个最小FreeRTOS相关源文件和两个头文件目录加入CMake后，于2026-08-08使用同一GNU Make构建流程完成首次完整链接：

```text
FLASH：2072 B / 512 KiB
RAM：2704 B / 64 KiB
text：2068 B
data：4 B
bss：2700 B
编译错误：0
链接错误：0
链接警告：1（Newlib libc_a-memset.o缺少.note.GNU-stack标记）
```

相较裸机基线，当前增加：

```text
FLASH：2072 - 1268 = 804 B
RAM：  2704 - 2568 = 136 B
```

当前增量还不是最终FreeRTOS Demo占用。因为应用尚未调用 `xTaskCreate()`、`vTaskStartScheduler()` 或 `pvPortMalloc()`，`--gc-sections` 已把未引用的任务创建、Heap和调度器启动路径裁掉，8 KiB `ucHeap[]` 尚未进入最终ELF。当前保留的关键符号包括 `xTaskIncrementTick()`、`vTaskSwitchContext()`、`pxCurrentTCB`、`xTickCount`、栈溢出Hook以及FreeRTOS提供的三个异常入口。

ELF符号检查确认：

```text
SVC_Handler       全局Text强符号，来自FreeRTOS ARM_CM3 port.c
PendSV_Handler    全局Text强符号，来自FreeRTOS ARM_CM3 port.c
SysTick_Handler   全局Text强符号，来自FreeRTOS ARM_CM3 port.c
```

它们已经覆盖启动文件中的同名弱默认处理函数。当前尚未启动调度器，因此这一结果只证明链接路由正确，不代表Tick和任务切换已经完成实机验证。

首次链接时，链接器因预编译Newlib `memset` 对象缺少 `.note.GNU-stack` 而把最终 `GNU_STACK` 标记为 `RWE`。用户随后把 `-Wl,-z,noexecstack` 加入CMake链接选项并重新生成固件。只读检查确认当前 `firmware.elf` 晚于CMake修改时间，`GNU_STACK` 已变为 `RW`，且固件尺寸仍为 `text=2068 B`、`data=4 B`、`bss=2700 B`。该链接策略已经生效；本工程不采用只隐藏警告的 `--no-warn-execstack`。

### LED Task隔离编译结果

用户已把 `User/app/led/app_led_task.c` 和 `User/app/led` 加入CMake，并完成Fresh构建。只读检查确认Task对象文件和最终ELF均晚于CMake配置：

```text
app_led_task.c.obj：已生成
最终ELF中的vLedTask：无
最终ELF中的xTaskCreate：无
最终ELF中的vTaskStartScheduler：无
text/data/bss：2068/4/2700 B
GNU_STACK：RW
```

源文件已经通过编译，但 `main.c` 尚未引用Task入口和调度器API，所以 `--gc-sections` 把相关函数从最终ELF裁掉。这一结果只验收Task源文件和包含路径，不代表Task已经创建或运行。

### 首个Task创建后的构建结果

用户改造 `main.c`，调用 `xTaskCreate()`和 `vTaskStartScheduler()`后完成构建。只读检查得到：

```text
FLASH：5024 B / 512 KiB，约0.96%
RAM：11016 B / 64 KiB，约16.81%
text：5016 B
data：8 B
bss：11008 B
GNU_STACK：RW
```

相较Task尚未被引用时：

```text
FLASH：5024 - 2072 = 增加2952 B
RAM：11016 - 2704 = 增加8312 B
```

其中 `ucHeap`位于 `0x20000104`，长度为 `0x2000`，即8192 B，结束地址为 `0x20002104`。链接脚本随后继续预留C运行库Heap和MSP主栈，RAM静态预留最高到 `0x20002B08`，没有超出 `0x20010000`。

最终ELF已经保留：

```text
xTaskCreate
vTaskStartScheduler
pvPortMalloc
vLedTask
vApplicationMallocFailedHook
vApplicationStackOverflowHook
pxCurrentTCB
xTickCount
ucHeap
```

向量表中的SVC、PendSV和SysTick入口分别指向FreeRTOS端口提供的 `SVC_Handler`、`PendSV_Handler`和 `SysTick_Handler`。向量表中函数地址最低位为1，表示Cortex-M Thumb状态，不是地址错误。构建和静态链接验收通过。当前 `app_led_task.c` 使用 `pdMS_TO_TICKS(2 * 1000)`，在1 kHz Tick下得到2000 Tick，即每2 s翻转一次LED。由于 `main.c` 后续增加的学习注释晚于当前ELF，虽然注释不改变机器指令，仍需在GDB验证前再构建一次，使DWARF源码行号与当前文件同步。

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

### FreeRTOS最小Demo的待执行验证

当前FreeRTOS Demo尚未记录真实硬件验收结果。由于 `main.c`最新注释晚于当前ELF，先执行一次增量构建，再启动OpenOCD服务器和GDB：

```powershell
cmake --build build -- -j8
openocd -f interface/cmsis-dap.cfg -c "transport select swd" -f target/stm32f1x.cfg -c "adapter speed 1000"
```

另开一个PowerShell终端：

```powershell
arm-none-eabi-gdb build/firmware.elf
```

本轮要依次验证 `main`、`xTaskCreate`、`vTaskStartScheduler`、`SVC_Handler`、`vLedTask`、`vTaskDelay`、`PendSV_Handler`和一次 `SysTick_Handler`。Malloc Failed Hook与Stack Overflow Hook在正常路径中不应命中。当前延时参数应为2000 Tick，PB5红灯预期每2 s翻转一次、完整亮灭周期约4 s。逐次断点和临时工具输出不写入学习文档，README只保留验证目标，完成后再记录最终结论。

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
PB5 红灯约每 2000 ms 翻转一次
```

本阶段不加入 CAN、W5500、MQTT、FTP、W25Q64 或 OTA 代码。最小 Demo 必须同时满足：构建和链接无警告、调度器不返回、LED 任务持续运行、错误 Hook 不触发、GDB 能观察任务以及 SysTick/PendSV/SVC 相关入口。

### 2. 总体步骤与当前状态

| 步骤 | 学习目标 | 当前状态 |
| --- | --- | --- |
| 0. 固化裸机基线 | 排除工具链、启动、链接、时钟、GPIO、下载器和 GDB 问题 | 已完成实机验证 |
| 1. 下载内核 | 从官方仓库取得并核验 FreeRTOS Kernel V11.3.0 | 已下载到 `resources/` 并完成版本核验 |
| 2. 认识内核组成 | 理解通用内核、处理器端口、内存管理器和应用配置的边界 | 已完成第一轮目录学习 |
| 3. 选择 Cortex-M3 端口 | 从完整上游快照中选择 `portable/GCC/ARM_CM3`，理解 SysTick、PendSV 和 SVC | 端口已接入构建，三个异常入口的ELF强符号已确认；实机异常路径待验证 |
| 4. 选择堆实现 | 选择 `heap_4.c`，理解任务创建所需的动态内存 | 已接入构建；创建Task后8 KiB `ucHeap`已进入ELF，运行时分配仍待GDB验证 |
| 5. 创建应用配置 | 编写工程自己的 `FreeRTOSConfig.h` | 已由用户创建；最小内核与应用Hook的只读语法检查通过 |
| 6. 接入 CMake | 只把最小 Demo 所需源文件和包含路径加入目标 | 已由用户完成并首次链接成功；`noexecstack`已验证生效，正式ELF的`GNU_STACK`为`RW` |
| 6A. 规范目录分层 | 把板级驱动统一放到 `User/bsp/<模块>/`，应用Task放到 `User/app/<功能域>/` | 规则已写入根目录`AGENTS.md`；LED BSP迁移和回归构建已完成，迁移后实机闪烁待复验 |
| 7. 改造入口程序 | 删除裸机 SysTick 忙等，创建 LED Task 并启动调度器 | 已由用户完成并成功生成ELF |
| 8. 构建和静态检查 | 检查异常符号、内存占用、映射文件和编译警告 | 符号、Heap、向量表、内存和GNU_STACK已复核；`main.c`最新注释晚于ELF，GDB前需再构建同步行号 |
| 9. 烧录和 GDB 验证 | 验证调度器、Tick、上下文切换、任务阻塞和错误 Hook | 已命中`main()`和`xTaskCreate()`；六个Task创建实参与配置一致，继续验证动态创建和调度路径 |
| 10. 最小 Demo 验收 | 连续运行并记录结果，形成可回退的 Git 基线 | 待执行 |

每完成一步，都应把实际命令、结果、遇到的问题和验证结论补充到本节，不能提前把计划写成已完成结果。

### 下一阶段FreeRTOS学习路线

首个LED Task完成硬件验收后，按以下顺序扩展，不在一次变更中同时启用全部同步机制：

```text
非阻塞按键BSP和轮询Task
    ↓
Queue传递按键事件
    ↓
Binary Semaphore完成EXTI到Task同步
    ↓
Mutex保护真实共享资源
```

历史 `User/Key/`和 `User/exti/`示例目前未加入CMake。接入前必须迁移到当前分层、去除等待按键释放的忙循环，并把EXTI的NVIC分组和中断优先级改为满足FreeRTOS `FromISR`边界的配置。具体原理和阶段验收见 [FreeRTOS学习.md](FreeRTOS学习.md#25-第二阶段按键queuesemaphore-与-mutex)。

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
4. 将完整上游快照保留在 `resources/` 中，用于学习、版本追溯和查阅未启用模块。
5. 从完整快照中选择最小 Demo 需要的内核文件、头文件、Cortex-M3 端口和内存管理器，由用户手动复制到 `firmware/Libraries/FreeRTOS-Kernel/`。
6. 保留许可证和版本记录，并确认固件依赖目录中没有多套一层目录或内嵌 `.git`。

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

本次实际下载和核验记录（2026-08-04）：

```text
完整上游快照：resources/开发板资料/FreeRTOS-Kernel-11.3.0/
固件目标目录：firmware/Libraries/FreeRTOS-Kernel/
```

核验结果：

- `include/task.h` 中的 `tskKERNEL_VERSION_NUMBER` 为 `"V11.3.0"`。
- 主版本、次版本和构建版本宏分别为 `11`、`3`、`0`。
- `History.txt` 首部记录 V11.3.0 于 2026 年 3 月发布。
- `include/FreeRTOS.h`、`tasks.c`、`list.c` 和 `queue.c` 存在。
- `portable/GCC/ARM_CM3/port.c` 与 `portmacro.h` 存在。
- `portable/MemMang/heap_4.c` 存在。
- `LICENSE.md`、官方 `README.md` 和 `History.txt` 存在。
- 下载目录没有 `.git`，不会形成嵌套 Git 仓库。

完整快照中的 `.gitmodules` 指向第三方扩展端口，但本项目需要的 `portable/GCC/ARM_CM3` 位于主内核仓库中且已存在，因此不需要为了本次 STM32F103 最小移植初始化第三方端口子模块。

最小内核文件选取验收记录（2026-08-04）：

- 用户已将完整 `include/` 复制到固件依赖目录。
- 已复制 `portable/GCC/ARM_CM3/port.c` 和 `portmacro.h`。
- 已复制 `portable/MemMang/heap_4.c`，未混入其他 Heap 实现。
- 已复制最小任务 Demo 需要的 `tasks.c` 和 `list.c`。
- 已保留 `LICENSE.md`、官方 `README.md` 和 `History.txt`。
- 目标目录中的版本宏仍为 V11.3.0。
- 未复制 `queue.c`、`timers.c`、`event_groups.c`、`stream_buffer.c` 或 `croutine.c`。
- 目标目录没有 `.git`，不会在 `firmware` 仓库中形成嵌套仓库。
- 此时只完成依赖文件准备；尚未加入 CMake，也尚未生成 `FreeRTOSConfig.h`，因此不能称为已经接入 FreeRTOS。

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

### 4.1 `FreeRTOS.h` 与 `FreeRTOSConfig.h` 的区别

`FreeRTOS.h` 和 `FreeRTOSConfig.h` 不是重复文件，也不是二选一关系：

| 文件 | 提供者 | 职责 | 是否修改官方文件 |
| --- | --- | --- | --- |
| `include/FreeRTOS.h` | FreeRTOS Kernel | 内核总入口头文件，定义基础类型、公共宏、内核对象结构和配置检查，并连接通用内核与端口层 | 不修改 |
| `User/FreeRTOSConfig.h` | 当前应用工程 | 告诉同一套内核如何服务于本项目，例如 CPU 时钟、Tick、任务优先级数量、Heap 大小、中断优先级和启用功能 | 由项目创建和维护 |

本项目复制的 V11.3.0 `FreeRTOS.h` 在第 58 行直接包含：

```c
#include "FreeRTOSConfig.h"
```

包含之后，`FreeRTOS.h` 会检查应用是否定义了必要配置。例如缺少任务最小栈、最大优先级数量或调度方式时，会通过 `#error` 停止编译。这说明内核源码故意不替具体产品决定这些参数。

典型包含关系为：

```text
main.c
  ├─ include "FreeRTOS.h"
  │      ├─ include "FreeRTOSConfig.h"   当前应用的选择
  │      └─ 连接 portable.h/portmacro.h   当前 CPU 端口能力
  └─ include "task.h"                    任务 API 声明
```

应用通常先包含 `FreeRTOS.h`，再包含所用功能的专用 API 头文件：

```c
#include "FreeRTOS.h"
#include "task.h"
```

`FreeRTOS.h` 不是“所有 API 函数声明的集合”。任务 API 主要在 `task.h`，队列 API 在 `queue.h`，信号量 API 在 `semphr.h`；`FreeRTOS.h` 提供这些模块共同依赖的基础定义和当前工程配置。

之所以不能把配置直接写进官方 `FreeRTOS.h`，是因为同一个 FreeRTOS Kernel 可以同时服务于完全不同的工程：一个 MCU 可能运行在 72 MHz、需要 1 kHz Tick 和 12 KiB RTOS Heap，另一个 MCU 可能运行在其他频率、使用不同 Tick 和静态内存。把项目参数与上游内核分离，还能避免升级内核时覆盖本地配置。

对当前目录理解的准确表述是：

1. `include/` 包含公共 API 头文件，也包含内核内部共享的类型、宏和端口接口；并非每个头文件都由应用直接调用。
2. `portable/` 适配的是“编译器 + CPU 架构”和内存分配策略。`portable/GCC/ARM_CM3` 负责 Cortex-M3 的 SysTick、PendSV、SVC、临界区和上下文切换，但不负责 STM32F103 的 GPIO、RCC 或具体外设初始化。
3. `portable/MemMang/heap_4.c` 是独立于 ARM_CM3 端口的 FreeRTOS 动态内存策略，用于从 SRAM 中为动态任务的 TCB、任务栈和其他内核对象分配空间。
4. `list.c` 实现内核内部链表；`tasks.c` 在这些链表之上实现任务创建、状态转换、延时和调度器核心。不能简单理解为两个互不相关的“链表管理”和“任务管理”模块。

### 4.2 `FreeRTOSConfig.h` 的六类配置

第一次阅读官方模板时不应按文件从上到下机械记忆，而应先把宏分成六类：

| 类别 | 解决的问题 | 典型宏 |
| --- | --- | --- |
| 时钟与 Tick | CPU 每秒运行多少周期，RTOS 多久产生一次系统节拍 | `configCPU_CLOCK_HZ`、`configTICK_RATE_HZ`、可选的 `configSYSTICK_CLOCK_HZ` |
| 调度策略 | 是否抢占、是否时间片轮转、允许多少级任务优先级 | `configUSE_PREEMPTION`、`configUSE_TIME_SLICING`、`configMAX_PRIORITIES` |
| 内存与任务栈 | 动态/静态创建方式、RTOS Heap 大小、Idle Task 最小栈 | `configSUPPORT_DYNAMIC_ALLOCATION`、`configTOTAL_HEAP_SIZE`、`configMINIMAL_STACK_SIZE` |
| Cortex-M 中断 | NVIC有效位、优先级分组、`BASEPRI`边界，以及哪些中断可以调用 FreeRTOS `FromISR` API | `configMAX_SYSCALL_INTERRUPT_PRIORITY`；当前 V11.3.0 ARM_CM3 端口自行把PendSV/SysTick设为最低优先级 |
| 调试和 Hook | 断言、栈溢出、分配失败、Idle/Tick 回调 | `configASSERT`、`configCHECK_FOR_STACK_OVERFLOW`、`configUSE_MALLOC_FAILED_HOOK` |
| 功能和 API 裁剪 | 是否编译互斥量、软件定时器，以及是否暴露某些任务 API | `configUSE_MUTEXES`、`configUSE_TIMERS`、`INCLUDE_vTaskDelay` |

这些宏大多是编译期开关，不是运行时变量。改变配置后需要重新编译内核源码，不能期望程序运行后再动态改变调度器结构。

### 4.3 时钟与 Tick：把 72 MHz 变成 RTOS 时间

当前 CMSIS 系统文件已经确认：

```text
SystemCoreClock = 72,000,000 Hz
```

`configCPU_CLOCK_HZ` 告诉 FreeRTOS Cortex-M3 端口 CPU 核心时钟频率。它可以使用已经由 CMSIS 维护的 `SystemCoreClock`，也可以写成经过板级验证的固定常量；后续建立配置文件时再结合依赖关系选择具体写法，不能同时维护两个可能不一致的时钟事实。

`configTICK_RATE_HZ` 表示每秒产生多少次 RTOS Tick。候选值 1000 Hz 的含义是：

```text
1 秒 / 1000 Tick = 每个 Tick 1 ms
```

V11.3.0 的 ARM_CM3 `port.c` 使用以下关系配置 SysTick：

```text
SysTick LOAD = configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ - 1
```

若 SysTick 使用 72 MHz 内核时钟，Tick 设为 1000 Hz，则：

```text
72,000,000 / 1000 - 1 = 71,999
```

SysTick 从 71,999 递减到 0，共计数 72,000 个时钟周期，因此每 1 ms 进入一次 Tick Handler。当前 ARM_CM3 端口在没有单独定义 `configSYSTICK_CLOCK_HZ` 时，会自动令它等于 `configCPU_CLOCK_HZ`；只有 SysTick 使用不同外部时钟源时才需要单独定义。

当任务执行：

```c
vTaskDelay(pdMS_TO_TICKS(500U));
```

在 1000 Hz Tick 下，`pdMS_TO_TICKS(500U)` 得到 500 Tick。任务会进入阻塞态，调度器可以运行其他任务；这与裸机 `delay_ms()` 占用 CPU 忙等 500 ms 有本质区别。

Tick 频率不是越高越好：频率越高，延时分辨率越细，但 Tick 中断和调度判断的 CPU 开销也越高。1 kHz 是本项目最小学习 Demo 的候选基线，最终结果需要通过实机构建和运行验证后记录。

### 4.4 调度策略和任务优先级

最小 Demo 计划使用抢占式调度：

- `configUSE_PREEMPTION=1`：更高优先级任务变为就绪态时，可以抢占当前低优先级任务。
- `configUSE_TIME_SLICING=1`：多个同优先级就绪任务可以在 Tick 到来时轮转。
- `configMAX_PRIORITIES`：定义可用任务优先级数量，而不是某个任务的实际优先级。

如果 `configMAX_PRIORITIES=5`，可用任务优先级为：

```text
0、1、2、3、4
```

FreeRTOS 任务优先级数值越大，任务优先级越高；Idle Task 固定使用 `tskIDLE_PRIORITY`，其数值为 0。这个规则与 Cortex-M NVIC 的“中断优先级数值越小、硬件优先级越高”正好相反，后续不能混淆：

```text
FreeRTOS Task：数值越大，优先级越高
Cortex-M IRQ： 数值越小，优先级越高
```

第一版只有一个 LED Task 和内核自动创建的 Idle Task。LED Task 后续可设置为 `tskIDLE_PRIORITY + 1`，使其高于 Idle Task；调用 `vTaskDelay()` 后 LED Task 阻塞，Idle Task 才获得运行机会。

### 4.5 `heap_4.c`、RTOS Heap 与任务栈

选用 `heap_4.c` 表示通过 FreeRTOS 的 `pvPortMalloc()`/`vPortFree()` 管理一块 RTOS 专用 Heap。动态创建一个任务时，内核通常需要从这里分配两块内存：

```text
任务控制块 TCB    保存任务状态、优先级、栈顶等信息
任务栈            保存局部变量、函数调用现场和切换时的寄存器
```

相关配置分工如下：

- `configSUPPORT_DYNAMIC_ALLOCATION=1`：允许 `xTaskCreate()` 动态创建任务。
- `configTOTAL_HEAP_SIZE`：决定 `heap_4.c` 管理的总空间。
- `configMINIMAL_STACK_SIZE`：主要决定内核自动创建的 Idle Task 的最小栈深度，不等于所有任务都必须使用这个栈大小。
- `xTaskCreate()` 的栈深度参数：决定该具体任务自己的任务栈大小。

在 Cortex-M3 端口中，`StackType_t` 是 32 位，因此栈深度单位通常是 4 字节的“字”，不是字节。例如：

```text
栈深度 128 × 4 字节 = 512 字节
```

`configTOTAL_HEAP_SIZE` 也不是 STM32F103 全部 64 KiB SRAM。SRAM 还要容纳：

- `.data` 和 `.bss` 全局/静态数据。
- FreeRTOS 的 Heap 数组。
- 中断和启动阶段使用的主栈 MSP。
- 当前链接脚本为 C 运行环境预留的 Heap/Stack 区域。

因此第一版即使选择一个保守的 RTOS Heap 候选值，也必须在链接完成后结合 `firmware.map` 和 `--print-memory-usage` 检查总 RAM，而不是只看 `configTOTAL_HEAP_SIZE`。当前只学习内存模型，具体数值将在创建配置文件时明确，并在实机验证后定稿。

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

- 当前代码用于FreeRTOS最小调度闭环和板级运行环境验证，不代表后续业务架构已经实现。
- FreeRTOS V11.3.0最小内核已经接入；`SysTick`已由ARM_CM3端口用于RTOS Tick，动态运行结果仍待用户在硬件上验证。
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
