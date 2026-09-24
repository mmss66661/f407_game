/**
 ****************************************************************************************************
 * @file        snake_game.h
 * @author      (本项目: 游戏机架构)
 * @version     V2.0
 * @date        2026-09-24
 * @brief       贪吃蛇游戏 —— 实现统一游戏接口(game_api)
 *
 *              核心机制:
 *                - 蛇在网格地图上移动, 吃到食物身体变长 + 得分
 *                - 撞墙 / 撞到自己则游戏结束
 *                - 每 +5 分速度加快一档
 *                - 最高分通过 save 模块持久化到 SD 卡 /SAVE/snake.sav
 *
 *              按键映射(USB 键盘 + 板载按键双输入, 由 input 层统一转换):
 *                ↑/W=上, ↓/S=下, ←/A=左, →/D=右
 *                空格/回车 = 重开; ESC 或长按 = 退出回菜单
 ****************************************************************************************************
 */

#ifndef __SNAKE_GAME_H
#define __SNAKE_GAME_H

#include "./SYSTEM/sys/sys.h"
#include "./BSP/LCD/lcd.h"
#include "./game_api.h"

/* ==================== 游戏运行状态 ==================== */
typedef enum
{
    SNAKE_RUNNING = 0,   /* 游戏进行中 */
    SNAKE_OVER,          /* 游戏结束(撞墙/撞自己) */
    SNAKE_QUIT,          /* 退出(返回菜单) */
} snake_state_t;

/* ==================== 对外接口 ==================== */

/* 获取贪吃蛇游戏的 game_t 实例(注册进菜单) */
const game_t *snake_get_game(void);

/* 游戏是否仍在运行(0=已退出) —— 供菜单判断 */
uint8_t snake_is_running(void);

#endif /* __SNAKE_GAME_H */
