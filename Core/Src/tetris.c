/**
 ****************************************************************************************************
 * @file        tetris.c
 * @author      (本项目: 游戏机架构)
 * @version     V1.0
 * @date        2026-09-24
 * @brief       俄罗斯方块游戏实现 —— 符合统一游戏接口(game_api)
 *
 *              玩法: 7 种方块下落, 左右移动/旋转, 填满一行消除。
 *              消行计分: 1行=100, 2行=300, 3行=500, 4行=800。
 *              每消 10 行升一级, 下落速度加快。
 ****************************************************************************************************
 */

#include "./tetris.h"
#include "./save.h"
#include "string.h"
#include "stdio.h"

/* ==================== 棋盘参数 ==================== */
#define T_COLS          16          /* 棋盘列数 */
#define T_ROWS          24          /* 棋盘行数 */
#define T_CELL          14          /* 每格像素(16*14=224 宽) */

/* 棋盘在屏幕上的偏移(480x800 竖屏) */
#define T_OFF_X         ((lcddev.width - T_COLS * T_CELL) / 2)   /* 128 */
#define T_OFF_Y         70

/* 存档文件名 */
#define TETRIS_SAVE_FILE  "tetris.sav"

/* ==================== 方块定义 ==================== */
/* 每种方块 4 个旋转状态, 每个状态 4 个格子, 用 4x4 位图表示(16 bit) */
/* 位图: bit(y*4+x), 最左上角为 (0,0) */
static const uint16_t SHAPES[7][4] =
{
    /* I 长条 */
    { 0x0F00, 0x2222, 0x00F0, 0x4444 },
    /* O 方块 */
    { 0x0660, 0x0660, 0x0660, 0x0660 },
    /* T */
    { 0x0E40, 0x4C40, 0x4E00, 0x4640 },
    /* S */
    { 0x06C0, 0x8C40, 0x6C00, 0x4620 },
    /* Z */
    { 0x0C60, 0x4C80, 0xC600, 0x2640 },
    /* L */
    { 0x0E80, 0xC440, 0x2E00, 0x4460 },
    /* J */
    { 0x0E20, 0x44C0, 0x8E00, 0x6440 },
};

/* 方块颜色(对应 7 种) */
static const uint16_t SHAPE_COLORS[7] =
{
    RED, YELLOW, MAGENTA, GREEN, BLUE, GRAYBLUE, BROWN,
};

/* ==================== 游戏状态 ==================== */
typedef struct
{
    uint8_t  board[T_ROWS][T_COLS];   /* 0=空, 非0=方块类型+1 */
    uint16_t board_color[T_ROWS][T_COLS];

    /* 当前活动方块 */
    int8_t   cur_shape;      /* 0~6 */
    int8_t   cur_rot;        /* 0~3 旋转状态 */
    int8_t   cur_x, cur_y;   /* 方块左上角在棋盘的位置(列,行) */

    int8_t   next_shape;     /* 下一个方块 */

    uint32_t score;
    uint32_t high_score;
    uint32_t level;          /* 等级(影响下落速度) */
    uint32_t lines;          /* 已消行数 */

    uint32_t tick;           /* 上次下落时间戳 */
    uint32_t fall_speed;     /* 下落周期 ms */

    uint8_t  state;          /* 0=running, 1=over, 2=quit */
} tetris_t;

static tetris_t g_t;

/* ==================== 内部函数声明 ==================== */
static void tetris_reset(void);
static void tetris_spawn(void);
static uint8_t tetris_collide(int8_t shape, int8_t rot, int8_t px, int8_t py);
static void tetris_draw_piece(int8_t shape, int8_t rot, int8_t px, int8_t py, uint16_t color);
static void tetris_lock_piece(void);
static void tetris_clear_lines(void);
static void tetris_draw_board(void);
static void tetris_draw_next(void);
static void tetris_draw_hud(void);
static void tetris_game_over_ui(void);
static void tetris_load_high_score(void);
static void tetris_save_high_score(void);
static uint32_t tetris_rand(void);
static void tetris_drop(void);

/* ==================== 伪随机数 ==================== */
static uint32_t tetris_rand_seed;

static uint32_t tetris_rand(void)
{
    tetris_rand_seed = tetris_rand_seed * 1103515245UL + 12345UL + (uint32_t)(SysTick->VAL);
    return (tetris_rand_seed >> 16) & 0x7FFF;
}

/* ==================== 最高分 ==================== */
static void tetris_load_high_score(void)
{
    g_t.high_score = 0;
    if (save_available())
    {
        save_read_u32(TETRIS_SAVE_FILE, &g_t.high_score);
    }
}

static void tetris_save_high_score(void)
{
    if (save_available())
    {
        save_write_u32(TETRIS_SAVE_FILE, g_t.high_score);
    }
}

/* ==================== 重置 ==================== */
static void tetris_reset(void)
{
    memset(g_t.board, 0, sizeof(g_t.board));
    memset(g_t.board_color, 0, sizeof(g_t.board_color));

    g_t.score = 0;
    g_t.level = 0;
    g_t.lines = 0;
    g_t.fall_speed = 500;     /* 初始 500ms/格 */
    g_t.next_shape = tetris_rand() % 7;
    g_t.state = 0;

    lcd_clear(WHITE);
    tetris_draw_board();
    tetris_draw_hud();
    tetris_draw_next();

    tetris_spawn();
}

/* ==================== 生成新方块 ==================== */
static void tetris_spawn(void)
{
    g_t.cur_shape = g_t.next_shape;
    g_t.cur_rot = 0;
    g_t.cur_x = (T_COLS - 4) / 2;   /* 4x4 方块居中 */
    g_t.cur_y = 0;
    g_t.next_shape = tetris_rand() % 7;

    g_t.tick = HAL_GetTick();

    /* 生成即碰撞 => 游戏结束 */
    if (tetris_collide(g_t.cur_shape, g_t.cur_rot, g_t.cur_x, g_t.cur_y))
    {
        g_t.state = 1;
        tetris_game_over_ui();
    }
    else
    {
        tetris_draw_piece(g_t.cur_shape, g_t.cur_rot, g_t.cur_x, g_t.cur_y, SHAPE_COLORS[g_t.cur_shape]);
        tetris_draw_next();
    }
}

/* ==================== 碰撞检测 ==================== */
static uint8_t tetris_collide(int8_t shape, int8_t rot, int8_t px, int8_t py)
{
    uint16_t bm = SHAPES[shape][rot];
    int8_t x, y;

    for (y = 0; y < 4; y++)
    {
        for (x = 0; x < 4; x++)
        {
            if (bm & (1 << (y * 4 + x)))
            {
                int8_t bx = px + x;
                int8_t by = py + y;

                if (bx < 0 || bx >= T_COLS || by >= T_ROWS)
                {
                    return 1;
                }
                if (by >= 0 && g_t.board[by][bx] != 0)
                {
                    return 1;
                }
            }
        }
    }
    return 0;
}

/* ==================== 绘制/擦除方块 ==================== */
static void tetris_draw_piece(int8_t shape, int8_t rot, int8_t px, int8_t py, uint16_t color)
{
    uint16_t bm = SHAPES[shape][rot];
    int8_t x, y;

    for (y = 0; y < 4; y++)
    {
        for (x = 0; x < 4; x++)
        {
            if (bm & (1 << (y * 4 + x)))
            {
                int16_t sx = T_OFF_X + (px + x) * T_CELL;
                int16_t sy = T_OFF_Y + (py + y) * T_CELL;
                if ((py + y) >= 0)
                {
                    lcd_fill(sx + 1, sy + 1, sx + T_CELL - 2, sy + T_CELL - 2, color);
                }
            }
        }
    }
}

/* ==================== 方块固定到棋盘 ==================== */
static void tetris_lock_piece(void)
{
    uint16_t bm = SHAPES[g_t.cur_shape][g_t.cur_rot];
    int8_t x, y;

    for (y = 0; y < 4; y++)
    {
        for (x = 0; x < 4; x++)
        {
            if (bm & (1 << (y * 4 + x)))
            {
                int8_t bx = g_t.cur_x + x;
                int8_t by = g_t.cur_y + y;
                if (by >= 0 && bx >= 0 && bx < T_COLS && by < T_ROWS)
                {
                    g_t.board[by][bx] = g_t.cur_shape + 1;
                    g_t.board_color[by][bx] = SHAPE_COLORS[g_t.cur_shape];
                }
            }
        }
    }

    tetris_clear_lines();
}

/* ==================== 消行 ==================== */
static void tetris_clear_lines(void)
{
    int8_t y, x;
    uint32_t cleared = 0;
    uint32_t add_score;

    for (y = T_ROWS - 1; y >= 0; y--)
    {
        uint8_t full = 1;
        for (x = 0; x < T_COLS; x++)
        {
            if (g_t.board[y][x] == 0)
            {
                full = 0;
                break;
            }
        }

        if (full)
        {
            /* 消去第 y 行, 上方整体下移 */
            int8_t yy;
            for (yy = y; yy > 0; yy--)
            {
                memcpy(g_t.board[yy], g_t.board[yy - 1], T_COLS);
                memcpy(g_t.board_color[yy], g_t.board_color[yy - 1], T_COLS * 2);
            }
            memset(g_t.board[0], 0, T_COLS);
            memset(g_t.board_color[0], 0, T_COLS * 2);

            cleared++;
            y++;    /* 重新检查同一行(下移后的新内容) */
        }
    }

    if (cleared == 0) return;

    /* 计分 */
    switch (cleared)
    {
        case 1: add_score = 100; break;
        case 2: add_score = 300; break;
        case 3: add_score = 500; break;
        default: add_score = 800; break;
    }
    g_t.score += add_score;
    g_t.lines += cleared;

    /* 每 10 行升一级 */
    g_t.level = g_t.lines / 10;

    /* 加速: 等级越高越快, 下限 80ms */
    if (g_t.fall_speed > 80)
    {
        g_t.fall_speed = 500 - g_t.level * 40;
        if (g_t.fall_speed < 80) g_t.fall_speed = 80;
    }

    if (g_t.score > g_t.high_score)
    {
        g_t.high_score = g_t.score;
    }

    /* 重绘棋盘 + HUD */
    tetris_draw_board();
    tetris_draw_hud();
}

/* ==================== 绘制棋盘(所有已固定的块) ==================== */
static void tetris_draw_board(void)
{
    int8_t x, y;

    /* 清空棋盘区 */
    lcd_fill(T_OFF_X, T_OFF_Y, T_OFF_X + T_COLS * T_CELL, T_OFF_Y + T_ROWS * T_CELL, WHITE);

    for (y = 0; y < T_ROWS; y++)
    {
        for (x = 0; x < T_COLS; x++)
        {
            if (g_t.board[y][x] != 0)
            {
                int16_t sx = T_OFF_X + x * T_CELL;
                int16_t sy = T_OFF_Y + y * T_CELL;
                lcd_fill(sx + 1, sy + 1, sx + T_CELL - 2, sy + T_CELL - 2, g_t.board_color[y][x]);
            }
        }
    }

    /* 边界 */
    lcd_draw_rectangle(T_OFF_X - 1, T_OFF_Y - 1,
                       T_OFF_X + T_COLS * T_CELL, T_OFF_Y + T_ROWS * T_CELL, BLACK);
}

/* ==================== 绘制下一个方块预览 ==================== */
static void tetris_draw_next(void)
{
    uint16_t bm;
    int8_t x, y;
    uint16_t px = T_OFF_X + T_COLS * T_CELL + 20;   /* 预览区在棋盘右侧 */
    uint16_t py = T_OFF_Y + 10;

    lcd_fill(px - 5, py - 5, px + 60, py + 80, WHITE);
    lcd_show_string(px - 5, py - 20, 100, 16, 16, "NEXT", BLUE);

    bm = SHAPES[g_t.next_shape][0];
    for (y = 0; y < 4; y++)
    {
        for (x = 0; x < 4; x++)
        {
            if (bm & (1 << (y * 4 + x)))
            {
                lcd_fill(px + x * 14 + 1, py + y * 14 + 1,
                         px + x * 14 + 12, py + y * 14 + 12, SHAPE_COLORS[g_t.next_shape]);
            }
        }
    }
}

/* ==================== 绘制 HUD(分数/等级/行数) ==================== */
static void tetris_draw_hud(void)
{
    char buf[48];
    uint16_t x = T_OFF_X;
    uint16_t y = 30;

    sprintf(buf, "TETRIS   SCORE:%lu", (unsigned long)g_t.score);
    lcd_show_string(x, y, lcddev.width, 16, 16, buf, RED);

    sprintf(buf, "HIGH:%lu  LV:%lu  LN:%lu",
            (unsigned long)g_t.high_score, (unsigned long)g_t.level, (unsigned long)g_t.lines);
    lcd_show_string(x, y + 20, lcddev.width, 16, 16, buf, BLUE);
}

/* ==================== 游戏结束界面 ==================== */
static void tetris_game_over_ui(void)
{
    uint16_t cx = lcddev.width / 2;
    uint16_t cy = lcddev.height / 2;
    char buf[48];

    lcd_fill(cx - 120, cy - 50, cx + 120, cy + 50, GRAYBLUE);

    lcd_show_string(cx - 96, cy - 40, 200, 16, 16, "GAME OVER", WHITE);

    sprintf(buf, "Score:%lu", (unsigned long)g_t.score);
    lcd_show_string(cx - 96, cy - 16, 200, 16, 16, buf, WHITE);

    sprintf(buf, "High :%lu", (unsigned long)g_t.high_score);
    lcd_show_string(cx - 96, cy + 8, 200, 16, 16, buf, YELLOW);

    lcd_show_string(cx - 96, cy + 30, 240, 16, 16, "Space:Restart", WHITE);
    lcd_show_string(cx - 96, cy + 50, 240, 16, 16, "Esc:Menu", WHITE);

    tetris_save_high_score();
}

/* ==================== 方块下落一格 ==================== */
static void tetris_drop(void)
{
    if (tetris_collide(g_t.cur_shape, g_t.cur_rot, g_t.cur_x, g_t.cur_y + 1))
    {
        /* 无法继续下落, 固定 */
        tetris_lock_piece();
        tetris_spawn();
    }
    else
    {
        /* 擦旧位置, 画新位置 */
        tetris_draw_piece(g_t.cur_shape, g_t.cur_rot, g_t.cur_x, g_t.cur_y, WHITE);
        g_t.cur_y++;
        tetris_draw_piece(g_t.cur_shape, g_t.cur_rot, g_t.cur_x, g_t.cur_y, SHAPE_COLORS[g_t.cur_shape]);
    }
}

/* ==================== game_api 接口实现 ==================== */

static void tetris_init(void)
{
    tetris_load_high_score();
    tetris_reset();
}

static void tetris_update(void)
{
    if (g_t.state != 0) return;

    uint32_t now = HAL_GetTick();
    if ((uint32_t)(now - g_t.tick) >= g_t.fall_speed)
    {
        g_t.tick = now;
        tetris_drop();
    }
}

static void tetris_on_input(game_cmd_t cmd)
{
    switch (g_t.state)
    {
        case 0:   /* running */
        {
            switch (cmd)
            {
                case CMD_LEFT:
                    if (!tetris_collide(g_t.cur_shape, g_t.cur_rot, g_t.cur_x - 1, g_t.cur_y))
                    {
                        tetris_draw_piece(g_t.cur_shape, g_t.cur_rot, g_t.cur_x, g_t.cur_y, WHITE);
                        g_t.cur_x--;
                        tetris_draw_piece(g_t.cur_shape, g_t.cur_rot, g_t.cur_x, g_t.cur_y, SHAPE_COLORS[g_t.cur_shape]);
                    }
                    break;

                case CMD_RIGHT:
                    if (!tetris_collide(g_t.cur_shape, g_t.cur_rot, g_t.cur_x + 1, g_t.cur_y))
                    {
                        tetris_draw_piece(g_t.cur_shape, g_t.cur_rot, g_t.cur_x, g_t.cur_y, WHITE);
                        g_t.cur_x++;
                        tetris_draw_piece(g_t.cur_shape, g_t.cur_rot, g_t.cur_x, g_t.cur_y, SHAPE_COLORS[g_t.cur_shape]);
                    }
                    break;

                case CMD_DOWN:
                    tetris_drop();
                    g_t.tick = HAL_GetTick();   /* 加速下落 */
                    break;

                case CMD_UP:     /* 旋转 */
                case CMD_ACTION:
                {
                    int8_t nr = (g_t.cur_rot + 1) & 3;
                    if (!tetris_collide(g_t.cur_shape, nr, g_t.cur_x, g_t.cur_y))
                    {
                        tetris_draw_piece(g_t.cur_shape, g_t.cur_rot, g_t.cur_x, g_t.cur_y, WHITE);
                        g_t.cur_rot = nr;
                        tetris_draw_piece(g_t.cur_shape, g_t.cur_rot, g_t.cur_x, g_t.cur_y, SHAPE_COLORS[g_t.cur_shape]);
                    }
                    break;
                }

                case CMD_QUIT:
                    g_t.state = 2;
                    break;

                default:
                    break;
            }
            break;
        }

        case 1:   /* over */
        {
            if (cmd == CMD_RESTART)
            {
                tetris_reset();
            }
            else if (cmd == CMD_QUIT)
            {
                g_t.state = 2;
            }
            break;
        }

        default:
            break;
    }
}

static void tetris_exit(void)
{
    tetris_save_high_score();
}

static const game_t s_tetris_game =
{
    .name     = "Tetris",
    .init     = tetris_init,
    .update   = tetris_update,
    .render   = NULL,
    .on_input = tetris_on_input,
    .exit     = tetris_exit,
};

const game_t *tetris_get_game(void)
{
    return &s_tetris_game;
}

uint8_t tetris_is_running(void)
{
    return (g_t.state != 2);
}
