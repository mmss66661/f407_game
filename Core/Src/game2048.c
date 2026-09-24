/**
 ****************************************************************************************************
 * @file        game2048.c
 * @author      (本项目: 游戏机架构)
 * @version     V1.0
 * @date        2026-09-25
 * @brief       2048 游戏实现 —— 符合统一游戏接口(game_api)
 *
 *              玩法: 4x4 格子滑动合并相同数字, 目标是合成 2048。
 *              每步滑动后随机生成一个 2(90%) 或 4(10%)。
 *              无法继续移动时游戏结束。
 ****************************************************************************************************
 */

#include "./game2048.h"
#include "./save.h"
#include "string.h"
#include "stdio.h"

/* ==================== 棋盘参数 ==================== */
/* 探索者 F407 4.3 寸屏实际显示高度可能为 480(横屏), 故棋盘需控制在高度 480 内 */
#define G2048_SIZE      4           /* 4x4 */
#define G2048_CELL      70          /* 每格像素 */
#define G2048_GAP       12          /* 格子间距 */

#define G2048_OFF_X     ((lcddev.width - (G2048_SIZE * G2048_CELL + (G2048_SIZE - 1) * G2048_GAP)) / 2)
#define G2048_OFF_Y     80          /* 4*70+3*12=316, 80+316=396 < 480 */

/* 存档文件名 */
#define G2048_SAVE_FILE  "game2048.sav"

/* ==================== 游戏状态 ==================== */
typedef struct
{
    uint32_t board[G2048_SIZE][G2048_SIZE];
    uint32_t score;
    uint32_t high_score;
    uint32_t best_tile;    /* 当前最大数字 */
    uint8_t  state;        /* 0=running, 1=win(可继续), 2=over, 3=quit */
    uint8_t  moved;        /* 本步是否有移动(用于生成新块) */
} game2048_t;

static game2048_t g_2048;

/* ==================== 伪随机数 ==================== */
static uint32_t g2048_rand_seed;

static uint32_t g2048_rand(void)
{
    g2048_rand_seed = g2048_rand_seed * 1103515245UL + 12345UL + (uint32_t)(SysTick->VAL);
    return (g2048_rand_seed >> 16) & 0x7FFF;
}

/* ==================== 最高分 ==================== */
static void g2048_load_high_score(void)
{
    g_2048.high_score = 0;
    if (save_available())
    {
        save_read_u32(G2048_SAVE_FILE, &g_2048.high_score);
    }
}

static void g2048_save_high_score(void)
{
    if (save_available())
    {
        save_write_u32(G2048_SAVE_FILE, g_2048.high_score);
    }
}

/* ==================== 数字颜色(按 2 的幂) ==================== */
static uint16_t g2048_tile_color(uint32_t v)
{
    switch (v)
    {
        case 0:     return GRAY;
        case 2:     return GRAYBLUE;
        case 4:     return BLUE;
        case 8:     return BROWN;
        case 16:    return MAGENTA;
        case 32:    return RED;
        case 64:    return YELLOW;
        case 128:   return GREEN;
        case 256:   return LIGHTGREEN;
        case 512:   return GRAYBLUE;
        case 1024:  return BLUE;
        case 2048:  return RED;
        default:    return MAGENTA;
    }
}

/* ==================== 绘制 ==================== */
static void g2048_draw_tile(uint8_t r, uint8_t c)
{
    uint16_t x = G2048_OFF_X + c * (G2048_CELL + G2048_GAP);
    uint16_t y = G2048_OFF_Y + r * (G2048_CELL + G2048_GAP);
    uint32_t v = g_2048.board[r][c];
    char buf[16];

    lcd_fill(x, y, x + G2048_CELL - 1, y + G2048_CELL - 1, g2048_tile_color(v));

    if (v != 0)
    {
        sprintf(buf, "%lu", (unsigned long)v);
        /* 根据数字位数调整字号和居中(格子 70px, 最大字号 24) */
        uint8_t size = (v < 100) ? 24 : (v < 1000 ? 16 : 12);
        uint16_t tx = x + (G2048_CELL - strlen(buf) * size) / 2;
        uint16_t ty = y + (G2048_CELL - size) / 2;
        lcd_show_string(tx, ty, G2048_CELL, size, size, buf, WHITE);
    }
}

static void g2048_draw_hud(void)
{
    char buf[48];
    sprintf(buf, "2048  SCORE:%lu", (unsigned long)g_2048.score);
    lcd_show_string(20, 20, lcddev.width, 16, 16, buf, RED);

    sprintf(buf, "HIGH:%lu  BEST:%lu", (unsigned long)g_2048.high_score, (unsigned long)g_2048.best_tile);
    lcd_show_string(20, 44, lcddev.width, 16, 16, buf, BLUE);
}

static void g2048_draw_board(void)
{
    uint8_t r, c;
    /* 清空棋盘区背景 */
    uint16_t bx = G2048_OFF_X - G2048_GAP;
    uint16_t by = G2048_OFF_Y - G2048_GAP;
    uint16_t bw = G2048_SIZE * G2048_CELL + (G2048_SIZE + 1) * G2048_GAP;
    uint16_t bh = G2048_SIZE * G2048_CELL + (G2048_SIZE + 1) * G2048_GAP;

    lcd_fill(bx, by, bx + bw, by + bh, GRAY);

    for (r = 0; r < G2048_SIZE; r++)
    {
        for (c = 0; c < G2048_SIZE; c++)
        {
            g2048_draw_tile(r, c);
        }
    }
}

/* ==================== 生成随机新块 ==================== */
static void g2048_spawn_random(void)
{
    uint8_t empty[G2048_SIZE * G2048_SIZE][2];
    uint8_t n = 0;
    uint8_t r, c;

    for (r = 0; r < G2048_SIZE; r++)
    {
        for (c = 0; c < G2048_SIZE; c++)
        {
            if (g_2048.board[r][c] == 0)
            {
                empty[n][0] = r;
                empty[n][1] = c;
                n++;
            }
        }
    }

    if (n == 0) return;

    uint8_t idx = g2048_rand() % n;
    r = empty[idx][0];
    c = empty[idx][1];
    /* 90% 生成 2, 10% 生成 4 */
    g_2048.board[r][c] = ((g2048_rand() % 10) < 9) ? 2 : 4;
    g2048_draw_tile(r, c);
}

/* ==================== 单行压缩合并(向左) ==================== */
static void g2048_merge_line(uint32_t line[G2048_SIZE])
{
    uint8_t i, j;

    /* 压缩: 去掉中间的 0 */
    uint32_t tmp[G2048_SIZE] = {0};
    uint8_t k = 0;
    for (i = 0; i < G2048_SIZE; i++)
    {
        if (line[i] != 0)
        {
            tmp[k++] = line[i];
        }
    }

    /* 合并相邻相同 */
    for (i = 0; i < G2048_SIZE - 1; i++)
    {
        if (tmp[i] != 0 && tmp[i] == tmp[i + 1])
        {
            tmp[i] *= 2;
            g_2048.score += tmp[i];
            if (tmp[i] > g_2048.best_tile) g_2048.best_tile = tmp[i];
            for (j = i + 1; j < G2048_SIZE - 1; j++)
            {
                tmp[j] = tmp[j + 1];
            }
            tmp[G2048_SIZE - 1] = 0;
        }
    }

    /* 写回 */
    for (i = 0; i < G2048_SIZE; i++)
    {
        line[i] = tmp[i];
    }
}

/* ==================== 移动(方向) ==================== */
static uint8_t g2048_move(uint8_t dir)   /* 0=up,1=down,2=left,3=right */
{
    uint8_t r, c;
    uint32_t before[G2048_SIZE][G2048_SIZE];
    uint8_t changed = 0;

    memcpy(before, g_2048.board, sizeof(g_2048.board));

    if (dir == 2)   /* left */
    {
        for (r = 0; r < G2048_SIZE; r++)
        {
            uint32_t line[G2048_SIZE];
            for (c = 0; c < G2048_SIZE; c++) line[c] = g_2048.board[r][c];
            g2048_merge_line(line);
            for (c = 0; c < G2048_SIZE; c++) g_2048.board[r][c] = line[c];
        }
    }
    else if (dir == 3)   /* right */
    {
        for (r = 0; r < G2048_SIZE; r++)
        {
            uint32_t line[G2048_SIZE];
            for (c = 0; c < G2048_SIZE; c++) line[G2048_SIZE - 1 - c] = g_2048.board[r][c];
            g2048_merge_line(line);
            for (c = 0; c < G2048_SIZE; c++) g_2048.board[r][G2048_SIZE - 1 - c] = line[c];
        }
    }
    else if (dir == 0)   /* up */
    {
        for (c = 0; c < G2048_SIZE; c++)
        {
            uint32_t line[G2048_SIZE];
            for (r = 0; r < G2048_SIZE; r++) line[r] = g_2048.board[r][c];
            g2048_merge_line(line);
            for (r = 0; r < G2048_SIZE; r++) g_2048.board[r][c] = line[r];
        }
    }
    else   /* down */
    {
        for (c = 0; c < G2048_SIZE; c++)
        {
            uint32_t line[G2048_SIZE];
            for (r = 0; r < G2048_SIZE; r++) line[G2048_SIZE - 1 - r] = g_2048.board[r][c];
            g2048_merge_line(line);
            for (r = 0; r < G2048_SIZE; r++) g_2048.board[r][G2048_SIZE - 1 - r] = line[r];
        }
    }

    for (r = 0; r < G2048_SIZE; r++)
        for (c = 0; c < G2048_SIZE; c++)
            if (g_2048.board[r][c] != before[r][c]) changed = 1;

    if (changed)
    {
        g_2048.moved = 1;
        if (g_2048.score > g_2048.high_score)
        {
            g_2048.high_score = g_2048.score;
        }
    }

    return changed;
}

/* ==================== 判断是否还能移动 ==================== */
static uint8_t g2048_can_move(void)
{
    uint8_t r, c;

    for (r = 0; r < G2048_SIZE; r++)
    {
        for (c = 0; c < G2048_SIZE; c++)
        {
            if (g_2048.board[r][c] == 0) return 1;
            if (c < G2048_SIZE - 1 && g_2048.board[r][c] == g_2048.board[r][c + 1]) return 1;
            if (r < G2048_SIZE - 1 && g_2048.board[r][c] == g_2048.board[r + 1][c]) return 1;
        }
    }
    return 0;
}

/* ==================== 重置 ==================== */
static void g2048_reset(void)
{
    memset(g_2048.board, 0, sizeof(g_2048.board));
    g_2048.score = 0;
    g_2048.best_tile = 0;
    g_2048.state = 0;
    g_2048.moved = 0;

    lcd_clear(WHITE);
    g2048_draw_board();
    g2048_draw_hud();

    /* 初始两个块 */
    g2048_spawn_random();
    g2048_spawn_random();
}

/* ==================== 游戏结束界面 ==================== */
static void g2048_game_over_ui(void)
{
    uint16_t cx = lcddev.width / 2;
    uint16_t cy = lcddev.height / 2;
    char buf[48];

    lcd_fill(cx - 120, cy - 50, cx + 120, cy + 50, GRAYBLUE);

    lcd_show_string(cx - 96, cy - 40, 200, 16, 16, "GAME OVER", WHITE);

    sprintf(buf, "Score:%lu", (unsigned long)g_2048.score);
    lcd_show_string(cx - 96, cy - 16, 200, 16, 16, buf, WHITE);

    sprintf(buf, "Best :%lu", (unsigned long)g_2048.best_tile);
    lcd_show_string(cx - 96, cy + 8, 200, 16, 16, buf, YELLOW);

    lcd_show_string(cx - 96, cy + 30, 240, 16, 16, "Space:Restart", WHITE);
    lcd_show_string(cx - 96, cy + 50, 240, 16, 16, "Esc:Menu", WHITE);

    g2048_save_high_score();
}

/* ==================== game_api 接口实现 ==================== */

static void g2048_init(void)
{
    g2048_load_high_score();
    g2048_reset();
}

static void g2048_update(void)
{
    /* 2048 是回合制, 无自动逻辑, 只在输入时推进 */
    (void)0;
}

static void g2048_on_input(game_cmd_t cmd)
{
    uint8_t dir;

    switch (g_2048.state)
    {
        case 0:   /* running */
        {
            switch (cmd)
            {
                case CMD_UP:    dir = 0; break;
                case CMD_DOWN:  dir = 1; break;
                case CMD_LEFT:  dir = 2; break;
                case CMD_RIGHT: dir = 3; break;
                case CMD_QUIT:  g_2048.state = 3; return;
                default:        return;
            }

            if (g2048_move(dir))
            {
                g2048_spawn_random();
                g2048_draw_hud();

                if (!g2048_can_move())
                {
                    g_2048.state = 2;
                    g2048_game_over_ui();
                }
            }
            break;
        }

        case 2:   /* over */
        {
            if (cmd == CMD_RESTART)
            {
                g2048_reset();
            }
            else if (cmd == CMD_QUIT)
            {
                g_2048.state = 3;
            }
            break;
        }

        default:
            break;
    }
}

static void g2048_exit(void)
{
    g2048_save_high_score();
}

static const game_t s_game2048 =
{
    .name     = "2048",
    .init     = g2048_init,
    .update   = g2048_update,
    .render   = NULL,
    .on_input = g2048_on_input,
    .exit     = g2048_exit,
};

const game_t *game2048_get_game(void)
{
    return &s_game2048;
}

uint8_t game2048_is_running(void)
{
    return (g_2048.state != 3);
}
