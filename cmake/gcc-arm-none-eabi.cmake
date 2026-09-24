set(CMAKE_SYSTEM_NAME               Generic)
set(CMAKE_SYSTEM_PROCESSOR          arm)

set(CMAKE_C_COMPILER_ID GNU)
set(CMAKE_CXX_COMPILER_ID GNU)

# ---------------------------------------------------------------------------
# 自动定位 arm-none-eabi 工具链
#
# 查找优先级：
#   1. 环境变量 STM32_CUBE_CLT_PATH 指向的目录（可选，用户可覆盖）
#   2. 常见安装路径（STM32CubeCLT / STM32CubeIDE 自带 GNU Tools）
#   3. 回退到 PATH 中的裸命令名 arm-none-eabi-gcc
#
# 这样无论是命令行、CLion、STM32CubeIDE 都能直接找到编译器，
# 无需手动把工具链 bin 目录加入系统 PATH。
# ---------------------------------------------------------------------------
if(NOT DEFINED TOOLCHAIN_BIN_DIR)
    # 1) 优先使用环境变量
    if(DEFINED ENV{STM32_CUBE_CLT_PATH})
        set(TOOLCHAIN_BIN_DIR "$ENV{STM32_CUBE_CLT_PATH}")
    else()
        # 2) 探测常见安装路径
        set(_candidate_dirs
            "C:/ST/STM32CubeCLT_1.18.0/GNU-tools-for-STM32/bin"
            "C:/ST/STM32CubeCLT/GNU-tools-for-STM32/bin"
            "E:/STM32CubeCLT_1.18.0/GNU-tools-for-STM32/bin"
            "E:/STM32CubeCLT/GNU-tools-for-STM32/bin"
        )
        foreach(_dir IN LISTS _candidate_dirs)
            if(EXISTS "${_dir}/arm-none-eabi-gcc.exe" OR EXISTS "${_dir}/arm-none-eabi-gcc")
                set(TOOLCHAIN_BIN_DIR "${_dir}")
                break()
            endif()
        endforeach()
    endif()
endif()

if(DEFINED TOOLCHAIN_BIN_DIR AND NOT TOOLCHAIN_BIN_DIR STREQUAL "")
    # 使用绝对路径（末尾统一带斜杠）
    string(REGEX REPLACE "[/\\\\]$" "" TOOLCHAIN_BIN_DIR "${TOOLCHAIN_BIN_DIR}")
    set(TOOLCHAIN_PREFIX "${TOOLCHAIN_BIN_DIR}/arm-none-eabi-")
    message(STATUS "Using ARM toolchain: ${TOOLCHAIN_PREFIX}gcc")
else()
    # 3) 回退到 PATH 中的裸命令名
    set(TOOLCHAIN_PREFIX "arm-none-eabi-")
endif()

# Windows 上编译器可执行文件带 .exe 扩展名，CMake 校验完整路径时必须补上
if(CMAKE_HOST_WIN32 AND DEFINED TOOLCHAIN_BIN_DIR AND NOT TOOLCHAIN_BIN_DIR STREQUAL "")
    set(TOOLCHAIN_SUFFIX ".exe")
else()
    set(TOOLCHAIN_SUFFIX "")
endif()

set(CMAKE_C_COMPILER                ${TOOLCHAIN_PREFIX}gcc${TOOLCHAIN_SUFFIX})
set(CMAKE_ASM_COMPILER              ${CMAKE_C_COMPILER})
set(CMAKE_CXX_COMPILER              ${TOOLCHAIN_PREFIX}g++${TOOLCHAIN_SUFFIX})
set(CMAKE_LINKER                    ${TOOLCHAIN_PREFIX}g++${TOOLCHAIN_SUFFIX})
set(CMAKE_OBJCOPY                   ${TOOLCHAIN_PREFIX}objcopy${TOOLCHAIN_SUFFIX})
set(CMAKE_SIZE                      ${TOOLCHAIN_PREFIX}size${TOOLCHAIN_SUFFIX})

set(CMAKE_EXECUTABLE_SUFFIX_ASM     ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_C       ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_CXX     ".elf")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# MCU specific flags
set(TARGET_FLAGS "-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard ")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TARGET_FLAGS}")
set(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS} -x assembler-with-cpp -MMD -MP")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -fdata-sections -ffunction-sections")

set(CMAKE_C_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_C_FLAGS_RELEASE "-Os -g0")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_CXX_FLAGS_RELEASE "-Os -g0")

set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fno-rtti -fno-exceptions -fno-threadsafe-statics")

set(CMAKE_EXE_LINKER_FLAGS "${TARGET_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T \"${CMAKE_SOURCE_DIR}/STM32F407XX_FLASH.ld\"")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --specs=nano.specs")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map=${CMAKE_PROJECT_NAME}.map -Wl,--gc-sections")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--print-memory-usage")
set(TOOLCHAIN_LINK_LIBRARIES "m")
