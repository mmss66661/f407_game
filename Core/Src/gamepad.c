/**
 ****************************************************************************************************
 * @file        gamepad.c
 * @author      (本项目: 游戏机架构)
 * @version     V1.0
 * @date        2026-09-25
 * @brief       手柄串口协议实现 —— 移植自 XBOX_HC05 项目的 20-byte frame
 *
 *              使用 USART3 (PB10=TX, PB11=RX) 接收 HC-05 蓝牙串口数据。
 *              流式帧同步 + CRC16/MODBUS 校验 + 字段解析。
 *
 *              注意: HAL_UART_MspInit / HAL_UART_RxCpltCallback 是全局唯一的
 *              weak 回调, 已在 usart.c 中定义(同时处理 USART1/USART3)。
 *              本文件只负责帧解析, 不定义这些回调, 避免重复定义冲突。
 ****************************************************************************************************
 */

#include "./gamepad.h"
#include "./SYSTEM/usart/usart.h"
#include "string.h"

/* ==================== USART3 引脚定义 ==================== */
#define GP_USART            USART3
#define GP_USART_IRQn       USART3_IRQn

#define GP_TX_GPIO_PORT     GPIOB
#define GP_TX_GPIO_PIN      GPIO_PIN_10
#define GP_TX_GPIO_AF       GPIO_AF7_USART3
#define GP_RX_GPIO_PORT     GPIOB
#define GP_RX_GPIO_PIN      GPIO_PIN_11
#define GP_RX_GPIO_AF       GPIO_AF7_USART3

#define GP_FRAME_SIZE       20u

/* ==================== 全局状态 ==================== */
gamepad_state_t g_gamepad;
volatile uint8_t g_gamepad_updated = 0;

UART_HandleTypeDef g_gamepad_uart;        /* USART3 句柄(供 usart.c 的 MSP/回调使用) */
static uint8_t g_gp_rx_byte;              /* 单字节接收缓冲 */
static uint8_t g_gp_frame[GP_FRAME_SIZE];
static uint8_t g_gp_used = 0;
static uint8_t g_gp_online = 0;

/* ==================== 工具函数 ==================== */
static uint16_t gamepad_read_u16_le(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint16_t gamepad_crc16_modbus(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFFu;
    uint16_t i;
    uint8_t bit;

    for (i = 0; i < length; i++)
    {
        crc ^= data[i];
        for (bit = 0; bit < 8; bit++)
        {
            crc = (crc & 1u) ? (uint16_t)((crc >> 1) ^ 0xA001u) : (uint16_t)(crc >> 1);
        }
    }
    return crc;
}

/* ==================== 帧解码 ==================== */
static uint8_t gamepad_decode_frame(const uint8_t *frame)
{
    if (frame[0] != 0xAAu || frame[1] != 0x55u || frame[2] != 0x01u)
    {
        return 0;
    }
    if (gamepad_crc16_modbus(&frame[2], 16u) != gamepad_read_u16_le(&frame[18]))
    {
        return 0;
    }

    g_gamepad.sequence = frame[3];
    g_gamepad.buttons  = gamepad_read_u16_le(&frame[4]);
    g_gamepad.lx = (int16_t)gamepad_read_u16_le(&frame[6]);
    g_gamepad.ly = (int16_t)gamepad_read_u16_le(&frame[8]);
    g_gamepad.rx = (int16_t)gamepad_read_u16_le(&frame[10]);
    g_gamepad.ry = (int16_t)gamepad_read_u16_le(&frame[12]);
    g_gamepad.lt = frame[14];
    g_gamepad.rt = frame[15];
    g_gamepad.hat_x = (int8_t)frame[16];
    g_gamepad.hat_y = (int8_t)frame[17];

    g_gamepad_updated = 1;
    g_gp_online = 1;
    return 1;
}

/* ==================== 流式字节喂入(状态机) ==================== */
void gamepad_feed_byte(uint8_t byte)
{
    if (g_gp_used == 0)
    {
        if (byte == 0xAAu)
        {
            g_gp_frame[g_gp_used++] = byte;
        }
        return;
    }

    if (g_gp_used == 1)
    {
        if (byte == 0x55u)
        {
            g_gp_frame[g_gp_used++] = byte;
        }
        else
        {
            g_gp_used = (byte == 0xAAu) ? 1u : 0u;
            if (g_gp_used == 1u)
            {
                g_gp_frame[0] = byte;
            }
        }
        return;
    }

    g_gp_frame[g_gp_used++] = byte;
    if (g_gp_used < GP_FRAME_SIZE)
    {
        return;
    }

    g_gp_used = 0;
    gamepad_decode_frame(g_gp_frame);
}

/* ==================== 初始化 ==================== */
void gamepad_init(void)
{
    g_gamepad_uart.Instance = GP_USART;
    g_gamepad_uart.Init.BaudRate = 9600;                    /* HC-05 默认 9600 8N1 */
    g_gamepad_uart.Init.WordLength = UART_WORDLENGTH_8B;
    g_gamepad_uart.Init.StopBits = UART_STOPBITS_1;
    g_gamepad_uart.Init.Parity = UART_PARITY_NONE;
    g_gamepad_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    g_gamepad_uart.Init.Mode = UART_MODE_TX_RX;
    g_gamepad_uart.Init.OverSampling = UART_OVERSAMPLING_16;

    HAL_UART_Init(&g_gamepad_uart);     /* 内部调用 HAL_UART_MspInit(usart.c 中已扩展 USART3 分支) */

    HAL_UART_Receive_IT(&g_gamepad_uart, &g_gp_rx_byte, 1);
}

/* ==================== 对外接口 ==================== */

/* USART3 接收完成回调(由 usart.c 的 HAL_UART_RxCpltCallback 转发调用) */
void gamepad_uart_rx_cplt(void)
{
    gamepad_feed_byte(g_gp_rx_byte);                              /* 喂给帧解析状态机 */
    HAL_UART_Receive_IT(&g_gamepad_uart, &g_gp_rx_byte, 1);       /* 继续接收下一字节 */
}

uint8_t gamepad_button(uint8_t bit)
{
    if (bit >= 16) return 0;
    return (g_gamepad.buttons & (1u << bit)) ? 1 : 0;
}

uint8_t gamepad_online(void)
{
    return g_gp_online;
}
