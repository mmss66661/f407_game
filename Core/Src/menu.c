/**
 ****************************************************************************************************
 * @file        menu.c
 * @author      (本项目: 游戏机架构)
 * @version     V1.0
 * @date        2026-09-24
 * @brief       游戏菜单 —— 游戏机主界面
 *
 *              开机进入菜单, 列出所有已注册的游戏, 用户上下选择、
 *              回车/空格启动。游戏退出后返回菜单。
 *
 *              菜单状态机:
 *                MENU_SHOW   -> 显示游戏列表, 上下选择
 *                MENU_START  -> 用户确认, 启动选中的游戏
 *
 *              新增游戏: 在 games_table[] 里注册一行即可。
 ****************************************************************************************************
 */

#include "./menu.h"
#include "./snake_game.h"
#include "./tetris.h"
#include "./input.h"
#include "./BSP/LCD/lcd.h"
#include "string.h"
#include "stdio.h"

/* ==================== 已注册的游戏表 ==================== */
/* 注意: 静态数组不能用函数调用初始化, 故在 menu_init 里填充 */
static const game_t *games_table[8];
static uint8_t s_game_count = 0;

/* 菜单状态 */
typedef enum
{
    MENU_SHOW = 0,     /* 显示菜单, 等待选择 */
    MENU_START,        /* 已选中, 准备启动游戏 */
} menu_state_t;

static uint8_t     s_sel = 0;        /* 当前选中的游戏索引 */
static menu_state_t s_state = MENU_SHOW;

/* ==================== 菜单命令处理 ==================== */
static void menu_on_cmd(game_cmd_t cmd)
{
    if (s_state != MENU_SHOW) return;

    switch (cmd)
    {
        case CMD_UP:
            if (s_sel > 0) s_sel--;
            menu_render();
            break;

        case CMD_DOWN:
            if (s_sel < s_game_count - 1) s_sel++;
            menu_render();
            break;

        case CMD_ACTION:
        case CMD_RESTART:
            /* 确认启动游戏 */
            s_state = MENU_START;
            break;

        default:
            break;
    }
}

/* ==================== 渲染菜单 ==================== */
void menu_render(void)
{
    uint8_t i;
    uint16_t x = 40;
    uint16_t y = 100;
    char buf[48];

    lcd_clear(WHITE);

    lcd_show_string(x, 40, lcddev.width, 32, 32, "GAME CONSOLE", RED);
    lcd_show_string(x, 80, lcddev.width, 16, 16, "Select a game:", BLUE);

    for (i = 0; i < s_game_count; i++)
    {
        uint16_t gy = y + i * 40;
        uint16_t color = (i == s_sel) ? RED : BLACK;

        /* 选中项加 ">" 前缀 */
        if (i == s_sel)
        {
            lcd_show_string(x, gy, 16, 16, 16, ">", RED);
        }

        sprintf(buf, "%d. %s", i + 1, games_table[i]->name);
        lcd_show_string(x + 20, gy, lcddev.width, 24, 24, buf, color);
    }

    lcd_show_string(x, y + s_game_count * 40 + 20, lcddev.width, 16, 16, "Up/Down: select", GRAY);
    lcd_show_string(x, y + s_game_count * 40 + 40, lcddev.width, 16, 16, "Enter: start", GRAY);
}

/* ==================== 初始化菜单 ==================== */
void menu_init(void)
{
    /* 注册所有游戏 */
    s_game_count = 0;
    games_table[s_game_count++] = snake_get_game();
    games_table[s_game_count++] = tetris_get_game();

    s_sel = 0;
    s_state = MENU_SHOW;
    menu_render();
}

/* ==================== 菜单主循环 ==================== */
const game_t *menu_loop(void)
{
    if (s_state == MENU_START)
    {
        /* 已确认启动, 返回选中的游戏, 并复位状态 */
        const game_t *g = games_table[s_sel];
        s_state = MENU_SHOW;
        return g;
    }

    return NULL;
}

/* ==================== 获取菜单命令处理(供 input 层注册) ==================== */
void menu_attach_input(void)
{
    input_set_handler(menu_on_cmd);
}
