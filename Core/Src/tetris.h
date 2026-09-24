/**
 ****************************************************************************************************
 * @file        tetris.h
 * @author      (本项目: 游戏机架构)
 * @version     V1.0
 * @date        2026-09-24
 * @brief       俄罗斯方块游戏 —— 实现统一游戏接口(game_api)
 *
 *              核心机制:
 *                - 7 种方块(IO T/S/Z L/J), 自动下落 + 左右移 + 旋转
 *                - 一行填满即消除, 消行越多得分越高, 下落速度随等级加快
 *                - 最高分通过 save 模块持久化到 SD 卡 /SAVE/tetris.sav
 *
 *              按键映射(USB 键盘 + 板载按键, 由 input 层统一转换):
 *                ←/→ 左右移动, ↓ 加速下落, ↑ 或 空格 旋转
 *                ESC = 退出回菜单
 ****************************************************************************************************
 */

#ifndef __TETRIS_H
#define __TETRIS_H

#include "./SYSTEM/sys/sys.h"
#include "./BSP/LCD/lcd.h"
#include "./game_api.h"

/* 获取俄罗斯方块的 game_t 实例 */
const game_t *tetris_get_game(void);

/* 游戏是否仍在运行(0=已退出) */
uint8_t tetris_is_running(void);

#endif /* __TETRIS_H */
