# STM32F103ZET6 固件工程

本目录承载野火 STM32F103 霸道 V2 的固件代码。当前先完成不依赖 FreeRTOS 的裸机 LED 冒烟测试，用它独立验证 ARM GCC 工具链、CMake + GNU Make 构建流程、GCC 启动文件、链接脚本、系统时钟和 GPIO。

当前仍处于项目 P0 环境验证阶段。裸机程序已经通过编译、链接和 ELF 静态检查，但尚未通过 CMSIS-DAP 下载到开发板进行实机验证；FreeRTOS、CAN、W5500、MQTT、FTP 和 Bootloader 尚未接入。

## 当前技术栈

| 范围 | 当前选型 | 状态 |
| --- | --- | --- |
| 开发板 | 野火 STM32F103 霸道 V2 | 已确认 |
| MCU | STM32F103ZET6，Cortex-M3 | 已确认 |
| 板级库 | CMSIS + STM32F10x Standard Peripheral Library V3.5.0 | 已接入 |
| RTOS | FreeRTOS Kernel V11.3.0 LTS | 已选型，尚未接入 |
| 交叉编译器 | GNU Tools for STM32 14.3.1，`arm-none-eabi-gcc` | 已验证 |
| 构建配置 | CMake 4.3.1 | 已验证 |
| 构建执行 | GNU Make 4.4.1 | 已验证 |
| 工具集合 | STM32CubeCLT 1.22.0 | 已安装 |
| 调试下载器 | CMSIS-DAP，SWD 接口 | 已选型，待实机验证 |
| 调试服务器 | OpenOCD | 待安装和配置 |
| 源码调试器 | `arm-none-eabi-gdb` | 待联调 |

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

当前尚未固定 OpenOCD 的 CMSIS-DAP 配置文件，因此本阶段不在 CMake 中加入未经验证的自动下载命令。下一步需要：

1. 安装并确认 `openocd --version` 可用。
2. 连接 CMSIS-DAP 的 SWDIO、SWCLK、GND 和目标电压参考。
3. 确认开发板 BOOT0 配置为从内部 Flash 启动。
4. 使用 OpenOCD 下载 `build/firmware.elf` 或 `build/firmware.hex`：
      ```
      openocd -f interface/cmsis-dap.cfg -c "transport select swd" -f target/stm32f1x.cfg -c "adapter speed 1000" -c "program build/firmware.elf verify reset exit"
      ```
5. 复位开发板，观察 PB5 对应的红灯是否约每 500 ms 翻转一次。
6. 记录 CMSIS-DAP 型号、OpenOCD 版本、配置文件和实测结果。

在红灯实机闪烁前，只能说明“构建链路通过”，不能说明板级移植已经完成。

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
FreeRTOS Kernel V11.3.0 LTS
portable/GCC/ARM_CM3
heap_4.c
FreeRTOSConfig.h
LED Task + vTaskDelay()
```

只有裸机点灯完成实机验证后，才加入 FreeRTOS。这样可以把启动文件、链接脚本、时钟和 GPIO 问题与调度器、SysTick、任务栈问题分开定位。

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
