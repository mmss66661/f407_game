/**
 ****************************************************************************************************
 * @file        gamepad.h
 * @author      (本项目: 游戏机架构)
 * @version     V1.0
 * @date        2026-09-25
 * @brief       手柄串口协议模块 —— 移植自 XBOX_HC05 项目
 *
 *              通过 HC-05 蓝牙串口(USART3)接收 Xbox 手柄状态帧。
 *              帧格式(20 字节):
 *                [0..1]  帧头 AA 55
 *                [2]     协议版本 01
 *                [3]     序号 0..255
 *                [4..5]  按键位图 uint16 小端
 *                [6..7]  LX int16 小端 (-32767..32767)
 *                [8..9]  LY int16 小端
 *                [10..11] RX int16 小端
 *                [12..13] RY int16 小端
 *                [14]    LT uint8 (0..255)
 *                [15]    RT uint8 (0..255)
 *                [16]    HAT X int8 (-1/0/1)
 *                [17]    HAT Y int8 (-1/0/1)
 *                [18..19] CRC16/MODBUS 小端(校验偏移 2..17)
 *
 *              按键位图 bit: 0=A 1=B 2=X 3=Y 4=LB 5=RB 6=BACK 7=START
 *                            8=LS 9=RS 10=GUIDE 11=上 12=下 13=左 14=右
 ****************************************************************************************************
 */

#ifndef __GAMEPAD_H
#define __GAMEPAD_H

#include "./SYSTEM/sys/sys.h"

/* ==================== 摇杆死区(阈值) ==================== */
#define GP_STICK_DEADZONE   8000        /* 摇杆中心死区, |lx/ly| < 8000 视为未推动 */

/* ==================== 按键位定义 ==================== */
#define GP_BTN_A         0
#define GP_BTN_B         1
#define GP_BTN_X         2
#define GP_BTN_Y         3
#define GP_BTN_LB        4
#define GP_BTN_RB        5
#define GP_BTN_BACK      6
#define GP_BTN_START     7
#define GP_BTN_LS        8
#define GP_BTN_RS        9
#define GP_BTN_GUIDE     10
#define GP_BTN_UP        11
#define GP_BTN_DOWN      12
#define GP_BTN_LEFT      13
#define GP_BTN_RIGHT     14

/* ==================== 手柄状态结构体 ==================== */
typedef struct
{
    uint8_t  sequence;      /* 帧序号 */
    uint16_t buttons;       /* 按键位图 */
    int16_t  lx, ly;        /* 左摇杆 */
    int16_t  rx, ry;        /* 右摇杆 */
    uint8_t  lt, rt;        /* 扳机 */
    int8_t   hat_x, hat_y;  /* 十字键(-1/0/1) */
} gamepad_state_t;

/* 当前手柄状态(解析成功后更新) */
extern gamepad_state_t g_gamepad;

/* 手柄数据是否有更新(解析到新帧时为真, 由上层消费后清零) */
extern volatile uint8_t g_gamepad_updated;

/* ==================== 对外接口 ==================== */

/* 初始化 USART3 用于接收 HC-05 手柄帧(9600 8N1, 中断接收) */
void gamepad_init(void);

/* 流式字节喂入(供 USART3 接收中断回调调用) */
void gamepad_feed_byte(uint8_t byte);

/* USART3 接收完成回调(由 usart.c 的 HAL_UART_RxCpltCallback 转发调用) */
void gamepad_uart_rx_cplt(void);

/* USART3 句柄(供 usart.c 的 HAL_UART_MspInit/RxCpltCallback 判断使用) */
extern UART_HandleTypeDef g_gamepad_uart;

/* 查询某个按键是否按下 */
uint8_t gamepad_button(uint8_t bit);

/* 查询手柄是否在线(收到过有效帧) */
uint8_t gamepad_online(void);

#endif /* __GAMEPAD_H */
