cmake_minimum_required(VERSION 3.22)                    #指明cmake的最低版本要求

project(stm32f103_firmware LANGUAGES C ASM)             #说明项目名称和使用的语言：C和汇编

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)          #if (源码目录 == 构建目录) STREQUAL：“string equal”
    message(FATAL_ERROR "In-source builds are not supported. Use: cmake -S . -B build")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   #这个 JSON 文件会记录每一个 .c/.cpp 文件真正使用的编译命令

# 这条命令创建了一个 CMake target
add_executable(firmware
    Libraries/CMSIS/startup/gcc/startup_stm32f103xe.S
    Libraries/CMSIS/system_stm32f10x.c
    Libraries/FWlib/src/stm32f10x_gpio.c
    Libraries/FWlib/src/stm32f10x_rcc.c
    
    # FreeRTOS Kernel V11.3.0 minimal sources.
    Libraries/FreeRTOS-Kernel/tasks.c
    Libraries/FreeRTOS-Kernel/list.c
    Libraries/FreeRTOS-Kernel/portable/GCC/ARM_CM3/port.c
    Libraries/FreeRTOS-Kernel/portable/MemMang/heap_4.c

    # Application-side FreeRTOS integration.
    User/rtos/freertos_hooks.c

    # Application tasks.
    User/app/led/app_led_task.c

    # Board support package.
    User/bsp/led/bsp_led.c

    User/main.c

)

# 设置目标属性
set_target_properties(firmware PROPERTIES
    SUFFIX ".elf"   # 生成的固件文件后缀名为 .elf
)

target_include_directories(firmware PRIVATE
    Libraries/CMSIS
    Libraries/FWlib/inc
    User
    Libraries/FreeRTOS-Kernel/include
    Libraries/FreeRTOS-Kernel/portable/GCC/ARM_CM3

    User/bsp/led
    User/app/led
)

target_compile_definitions(firmware PRIVATE
    STM32F10X_HD
    USE_STDPERIPH_DRIVER
)

# 给 firmware 这个 target 的“源文件编译过程”
# 添加下面这些 GCC 参数。
target_compile_options(firmware PRIVATE
    -mcpu=cortex-m3         # CPU 是 Cortex-M3
    -mthumb                 # 生成 Thumb 指令
    -mfloat-abi=soft
    -ffunction-sections     # 每个函数尽量单独放一个 section
    -fdata-sections         # 每份数据也尽量单独放 section
    -fno-common             # 不允许全局变量在多个文件中重复定义
    -ffreestanding          # 裸机环境
    -Wa,--noexecstack       # Wa: 传给汇编器的参数 ，让当前工程新生成的单个.o声明不需要可执行栈
    -Wall
    -Wextra
    $<$<COMPILE_LANGUAGE:C>:-std=c11>
    $<$<CONFIG:Debug>:-Og;-g3>
    $<$<CONFIG:Release>:-Os>
)

# .c / .S
#       ↓
# arm-none-eabi-gcc
# + 上面这些参数
#       ↓
# .o


# target_link_options() 设置的就是链接阶段的参数。
target_link_options(firmware PRIVATE
    -mcpu=cortex-m3
    -mthumb
    -mfloat-abi=soft
    -nostartfiles       # 不用 GCC 默认启动文件
    --specs=nano.specs
    --specs=nosys.specs
    -Wl,-z,noexecstack
# 告诉链接器：使用指定的 linker script
# 链接脚本解决的是：
# .text、.data、.bss、栈、堆等最终应该放到 STM32 的哪个地址。
    -T${CMAKE_CURRENT_SOURCE_DIR}/linker/STM32F103ZETx_FLASH.ld     # 指定链接脚本，这是 STM32 链接最关键的一项
    -Wl,--gc-sections       # wl:把参数传给链接器，丢弃未使用的函数和数据
    -Wl,--print-memory-usage    # 参数含义：构建结束时打印 Flash/RAM 使用情况
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/firmware.map,--cref
)

# ELF 生成之后再做额外工作
add_custom_command(TARGET firmware POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex
            $<TARGET_FILE:firmware>
            ${CMAKE_CURRENT_BINARY_DIR}/firmware.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:firmware>
            ${CMAKE_CURRENT_BINARY_DIR}/firmware.bin
    COMMAND ${CMAKE_SIZE} --format=berkeley $<TARGET_FILE:firmware>
    COMMENT "Generating firmware.hex and firmware.bin"
)
