/**
 ****************************************************************************************************
 * @file        menu.h
 * @author      (本项目: 游戏机架构)
 * @version     V1.0
 * @date        2026-09-24
 * @brief       游戏菜单 —— 列出所有游戏, 选卡启动
 ****************************************************************************************************
 */

#ifndef __MENU_H
#define __MENU_H

#include "./SYSTEM/sys/sys.h"
#include "./game_api.h"

/* 初始化菜单 */
void menu_init(void);

/* 渲染菜单 */
void menu_render(void);

/* 把菜单注册为当前输入命令的接收者 */
void menu_attach_input(void);

/* 菜单主循环(一帧调用一次), 返回用户选中的游戏或 NULL */
const game_t *menu_loop(void);

#endif /* __MENU_H */
