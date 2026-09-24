/**
 ****************************************************************************************************
 * @file        breakout.h
 * @author      (本项目: 游戏机架构)
 * @version     V1.0
 * @date        2026-09-25
 * @brief       打砖块游戏 —— 实现统一游戏接口(game_api)
 *
 *              核心机制:
 *                - 底部挡板接住反弹的球, 用球打碎上方的砖块
 *                - 砖块全部打碎或球掉落即结束, 每关砖块排布随机
 *                - 最高分通过 save 模块持久化到 SD 卡 /SAVE/breakout.sav
 *
 *              按键映射(USB 键盘 + 板载按键, 由 input 层统一转换):
 *                ←/→ 移动挡板, 空格/回车 发球或重开, ESC = 退出回菜单
 ****************************************************************************************************
 */

#ifndef __BREAKOUT_H
#define __BREAKOUT_H

#include "./SYSTEM/sys/sys.h"
#include "./BSP/LCD/lcd.h"
#include "./game_api.h"

/* 获取打砖块的 game_t 实例 */
const game_t *breakout_get_game(void);

/* 游戏是否仍在运行(0=已退出) */
uint8_t breakout_is_running(void);

#endif /* __BREAKOUT_H */
