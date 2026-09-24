cmake_minimum_required(VERSION 3.22)
# Enable CMake support for ASM and C languages
enable_language(C ASM)
# STM32CubeMX generated symbols (macros)
set(MX_Defines_Syms 
	USE_HAL_DRIVER 
	STM32F407xx
    USE_USB_FS
    $<$<CONFIG:Debug>:DEBUG>
)

# STM32CubeMX generated include paths
set(MX_Include_Dirs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/CMSIS/Device/ST/STM32F4xx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/CMSIS/Include
    # 正点原子 BSP/SYSTEM 头文件根目录 (对应源工程 MDK 的 ..\..\Drivers)
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers
    # 正点原子 MALLOC 头文件根目录 (对应源工程 MDK 的 ..\..\Middlewares)
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares
    # FatFs 文件系统头文件目录
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/FATFS/source
    # USB Host Library 头文件目录 (对应源工程 MDK 的 USB 相关路径)
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/USB/USB_APP
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/USB/STM32_USB_Host_Library/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/USB/STM32_USB_Host_Library/Class/HID/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/USB/STM32_USB_Host_Library/Class/MSC/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/USB/STM32_USB_Host_Library/Class/CDC/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/USB/STM32_USB_Host_Library/Class/AUDIO/Inc
)

# STM32CubeMX generated application sources
set(MX_Application_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/snake_game.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/tetris.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/breakout.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/game2048.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/menu.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/save.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/input.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/gamepad.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/stm32f4xx_it.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/stm32f4xx_hal_msp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/sysmem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/syscalls.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../startup_stm32f407xx.s
)

# STM32 HAL/LL Drivers (完整 HAL 驱动, 覆盖工程用到的所有外设)
set(STM32_Drivers_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/system_stm32f4xx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ramfunc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_exti.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_sram.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_usart.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_hcd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_sd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_fsmc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_usb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_sdmmc.c
)

# 正点原子 BSP / SYSTEM / MALLOC 源文件
set(ALIENTEK_BSP_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/BSP/LED/led.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/BSP/LCD/lcd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/BSP/SRAM/sram.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/BSP/KEY/key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/BSP/SDIO/sdio_sdcard.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/SYSTEM/sys/sys.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/SYSTEM/delay/delay.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/SYSTEM/usart/usart.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/MALLOC/malloc.c
)

# USB Host Library 源文件 (Core + HID Class + 应用层)
set(USB_HOST_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/USB/STM32_USB_Host_Library/Core/Src/usbh_core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/USB/STM32_USB_Host_Library/Core/Src/usbh_ctlreq.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/USB/STM32_USB_Host_Library/Core/Src/usbh_ioreq.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/USB/STM32_USB_Host_Library/Core/Src/usbh_pipes.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/USB/STM32_USB_Host_Library/Class/HID/Src/usbh_hid.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/USB/STM32_USB_Host_Library/Class/HID/Src/usbh_hid_keybd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/USB/STM32_USB_Host_Library/Class/HID/Src/usbh_hid_mouse.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/USB/STM32_USB_Host_Library/Class/HID/Src/usbh_hid_parser.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/USB/USB_APP/usbh_conf.c
)

# FatFs 文件系统源文件 (游戏存档用)
set(FATFS_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/FATFS/source/ff.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/FATFS/source/diskio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/FATFS/source/ffsystem.c
)

# Link directories setup
set(MX_LINK_DIRS

)
# Project static libraries
set(MX_LINK_LIBS 
    STM32_Drivers
    ${TOOLCHAIN_LINK_LIBRARIES}
    
)
# Interface library for includes and symbols
add_library(stm32cubemx INTERFACE)
target_include_directories(stm32cubemx INTERFACE ${MX_Include_Dirs})
target_compile_definitions(stm32cubemx INTERFACE ${MX_Defines_Syms})

# Create STM32_Drivers static library
add_library(STM32_Drivers OBJECT)
target_sources(STM32_Drivers PRIVATE
    ${STM32_Drivers_Src}
    ${ALIENTEK_BSP_Src}
    ${USB_HOST_Src}
    ${FATFS_Src}
)
target_link_libraries(STM32_Drivers PUBLIC stm32cubemx)


# Add STM32CubeMX generated application sources to the project
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${MX_Application_Src})

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE ${MX_LINK_DIRS})

# Add libraries to the project
target_link_libraries(${CMAKE_PROJECT_NAME} ${MX_LINK_LIBS})

# Add the map file to the list of files to be removed with 'clean' target
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES ADDITIONAL_CLEAN_FILES ${CMAKE_PROJECT_NAME}.map)

# Validate that STM32CubeMX code is compatible with C standard
if((CMAKE_C_STANDARD EQUAL 90) OR (CMAKE_C_STANDARD EQUAL 99))
    message(ERROR "Generated code requires C11 or higher")
endif()
