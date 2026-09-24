/**
 ****************************************************************************************************
 * @file        breakout.c
 * @author      (本项目: 游戏机架构)
 * @version     V1.0
 * @date        2026-09-25
 * @brief       打砖块游戏实现 —— 符合统一游戏接口(game_api)
 *
 *              玩法: 移动底部挡板接球, 反弹打碎上方砖块。
 *              球从挡板反弹, 碰到砖块即击碎并得分, 球掉落则失去一条命。
 *              3 条命用尽即游戏结束, 清空一关则进入下一关(速度加快)。
 ****************************************************************************************************
 */

#include "./breakout.h"
#include "./save.h"
#include "string.h"
#include "stdio.h"

/* ==================== 游戏参数 ==================== */
#define B_ROWS          5           /* 砖块行数 */
#define B_COLS          8           /* 砖块列数 */
#define B_BRICK_W       52          /* 砖块宽 */
#define B_BRICK_H       22          /* 砖块高 */
#define B_BRICK_GAP     4           /* 砖块间距 */

#define B_OFF_X         ((lcddev.width - B_COLS * (B_BRICK_W + B_BRICK_GAP)) / 2)
#define B_OFF_Y         90          /* 砖块区起始 Y */

#define B_PADDLE_W      70          /* 挡板宽 */
#define B_PADDLE_H      10          /* 挡板高 */
#define B_PADDLE_Y      (lcddev.height - 40)  /* 挡板 Y */

#define B_BALL_R        4           /* 球半径 */

#define B_MAX_LIVES     3

/* 存档文件名 */
#define BREAKOUT_SAVE_FILE  "breakout.sav"

/* ==================== 游戏状态 ==================== */
typedef struct
{
    uint8_t  bricks[B_ROWS][B_COLS];  /* 1=存在, 0=已碎 */
    uint16_t brick_color[B_ROWS];     /* 每行砖块颜色 */

    /* 球 */
    int16_t  ball_x, ball_y;          /* 球心坐标(定点: << 6) */
    int16_t  ball_vx, ball_vy;        /* 速度(定点) */

    /* 挡板 */
    int16_t  paddle_x;                /* 挡板左端 X */

    uint32_t score;
    uint32_t high_score;
    uint32_t lives;
    uint32_t level;

    uint8_t  launched;                /* 球是否已发出(0=吸附在挡板上) */

    uint32_t tick;                    /* 上次更新时间戳 */

    uint8_t  state;                   /* 0=running, 1=over, 2=quit */
} breakout_t;

static breakout_t g_b;

/* 行颜色(从上到下) */
static const uint16_t BRICK_COLORS[B_ROWS] =
{
    RED, YELLOW, GREEN, BLUE, MAGENTA,
};

/* 前置声明(breakout_update_ball 会调用它) */
static void breakout_game_over_ui(void);

/* ==================== 伪随机数 ==================== */
static uint32_t breakout_rand_seed;

static uint32_t breakout_rand(void)
{
    breakout_rand_seed = breakout_rand_seed * 1103515245UL + 12345UL + (uint32_t)(SysTick->VAL);
    return (breakout_rand_seed >> 16) & 0x7FFF;
}

/* ==================== 最高分 ==================== */
static void breakout_load_high_score(void)
{
    g_b.high_score = 0;
    if (save_available())
    {
        save_read_u32(BREAKOUT_SAVE_FILE, &g_b.high_score);
    }
}

static void breakout_save_high_score(void)
{
    if (save_available())
    {
        save_write_u32(BREAKOUT_SAVE_FILE, g_b.high_score);
    }
}

/* ==================== 绘制 ==================== */
static void breakout_draw_brick(uint8_t row, uint8_t col, uint16_t color)
{
    uint16_t x = B_OFF_X + col * (B_BRICK_W + B_BRICK_GAP);
    uint16_t y = B_OFF_Y + row * (B_BRICK_H + B_BRICK_GAP);
    lcd_fill(x, y, x + B_BRICK_W - 1, y + B_BRICK_H - 1, color);
}

static void breakout_draw_paddle(void)
{
    uint16_t y = B_PADDLE_Y;
    lcd_fill(g_b.paddle_x, y, g_b.paddle_x + B_PADDLE_W - 1, y + B_PADDLE_H - 1, GRAYBLUE);
}

static void breakout_erase_paddle(void)
{
    uint16_t y = B_PADDLE_Y;
    lcd_fill(g_b.paddle_x, y, g_b.paddle_x + B_PADDLE_W - 1, y + B_PADDLE_H - 1, WHITE);
}

static void breakout_draw_ball(void)
{
    lcd_fill_circle(g_b.ball_x >> 6, g_b.ball_y >> 6, B_BALL_R, BLACK);
}

static void breakout_erase_ball(void)
{
    lcd_fill_circle(g_b.ball_x >> 6, g_b.ball_y >> 6, B_BALL_R, WHITE);
}

static void breakout_draw_hud(void)
{
    char buf[48];
    sprintf(buf, "BREAKOUT  SCORE:%lu", (unsigned long)g_b.score);
    lcd_show_string(20, 20, lcddev.width, 16, 16, buf, RED);

    sprintf(buf, "HIGH:%lu  LIFE:%lu  LV:%lu",
            (unsigned long)g_b.high_score, (unsigned long)g_b.lives, (unsigned long)g_b.level);
    lcd_show_string(20, 40, lcddev.width, 16, 16, buf, BLUE);

    sprintf(buf, "LEFT:%lu", (unsigned long)(g_b.lives - 1));
    lcd_show_string(20, 60, lcddev.width, 16, 16, buf, GRAY);
}

static void breakout_draw_board(void)
{
    uint8_t r, c;
    for (r = 0; r < B_ROWS; r++)
    {
        for (c = 0; c < B_COLS; c++)
        {
            if (g_b.bricks[r][c])
            {
                breakout_draw_brick(r, c, g_b.brick_color[r]);
            }
        }
    }
}

/* ==================== 重置 ==================== */
static void breakout_reset(void)
{
    uint8_t r, c;

    memset(g_b.bricks, 0, sizeof(g_b.bricks));

    g_b.score = 0;
    g_b.lives = B_MAX_LIVES;
    g_b.level = 1;

    for (r = 0; r < B_ROWS; r++)
    {
        g_b.brick_color[r] = BRICK_COLORS[r];
        for (c = 0; c < B_COLS; c++)
        {
            /* 每关随机留一些空格, 增加变化 */
            g_b.bricks[r][c] = ((breakout_rand() % 10) < 8) ? 1 : 0;
        }
    }

    /* 挡板居中 */
    g_b.paddle_x = (lcddev.width - B_PADDLE_W) / 2;

    /* 球吸附在挡板中央 */
    g_b.ball_x = (g_b.paddle_x + B_PADDLE_W / 2) << 6;
    g_b.ball_y = (B_PADDLE_Y - B_BALL_R - 2) << 6;
    g_b.ball_vx = 0;
    g_b.ball_vy = 0;
    g_b.launched = 0;

    g_b.state = 0;

    lcd_clear(WHITE);
    breakout_draw_board();
    breakout_draw_paddle();
    breakout_draw_ball();
    breakout_draw_hud();
}

/* ==================== 发球 ==================== */
static void breakout_launch(void)
{
    /* 向上偏左或偏右随机发球, 速度随等级加快 */
    int16_t base = 2 + (g_b.level - 1);   /* 定点速度 */
    if (base > 5) base = 5;

    g_b.ball_vx = (breakout_rand() % 2) ? (base << 6) : -(base << 6);
    g_b.ball_vy = -(base << 6);
    g_b.launched = 1;
}

/* ==================== 球运动与碰撞 ==================== */
static void breakout_update_ball(void)
{
    int16_t nx, ny;

    if (!g_b.launched) return;

    /* 更新位置(定点运算) */
    nx = g_b.ball_x + g_b.ball_vx;
    ny = g_b.ball_y + g_b.ball_vy;

    /* 左右墙反弹 */
    if ((nx >> 6) - B_BALL_R < 0)
    {
        nx = B_BALL_R << 6;
        g_b.ball_vx = -g_b.ball_vx;
    }
    else if ((nx >> 6) + B_BALL_R > lcddev.width)
    {
        nx = (lcddev.width - B_BALL_R) << 6;
        g_b.ball_vx = -g_b.ball_vx;
    }

    /* 顶墙反弹 */
    if ((ny >> 6) - B_BALL_R < 0)
    {
        ny = B_BALL_R << 6;
        g_b.ball_vy = -g_b.ball_vy;
    }

    /* 底部: 掉落 */
    if ((ny >> 6) + B_BALL_R > lcddev.height)
    {
        /* 失去一条命 */
        if (g_b.lives > 0) g_b.lives--;

        if (g_b.lives == 0)
        {
            g_b.state = 1;
            breakout_game_over_ui();
            return;
        }

        /* 重新吸附到挡板 */
        g_b.launched = 0;
        g_b.ball_vx = 0;
        g_b.ball_vy = 0;
        g_b.ball_x = (g_b.paddle_x + B_PADDLE_W / 2) << 6;
        g_b.ball_y = (B_PADDLE_Y - B_BALL_R - 2) << 6;
        breakout_draw_ball();
        breakout_draw_hud();
        return;
    }

    /* 挡板反弹 */
    {
        int16_t bx = nx >> 6;
        int16_t by = ny >> 6;
        if ((by + B_BALL_R) >= B_PADDLE_Y &&
            (by + B_BALL_R) <= B_PADDLE_Y + B_PADDLE_H + 2 &&
            bx >= g_b.paddle_x - B_BALL_R &&
            bx <= g_b.paddle_x + B_PADDLE_W + B_BALL_R &&
            g_b.ball_vy > 0)
        {
            /* 根据撞击点偏移改变水平速度 */
            int16_t hit = bx - (g_b.paddle_x + B_PADDLE_W / 2);  /* -35~35 */
            int16_t speed = g_b.ball_vy < 0 ? -g_b.ball_vy : g_b.ball_vy;
            g_b.ball_vy = -speed;
            g_b.ball_vx = (hit * (speed >> 5));   /* 按偏移调整水平速度 */
            ny = (B_PADDLE_Y - B_BALL_R) << 6;
        }
    }

    /* 砖块碰撞 */
    {
        int16_t bx = nx >> 6;
        int16_t by = ny >> 6;
        int8_t cr = (by - B_OFF_Y) / (B_BRICK_H + B_BRICK_GAP);
        int8_t cc = (bx - B_OFF_X) / (B_BRICK_W + B_BRICK_GAP);

        if (cr >= 0 && cr < B_ROWS && cc >= 0 && cc < B_COLS && g_b.bricks[cr][cc])
        {
            g_b.bricks[cr][cc] = 0;
            breakout_draw_brick(cr, cc, WHITE);   /* 擦除砖块 */

            g_b.score += 10 * g_b.level;
            if (g_b.score > g_b.high_score)
            {
                g_b.high_score = g_b.score;
            }
            breakout_draw_hud();

            /* 反弹方向 */
            g_b.ball_vy = -g_b.ball_vy;

            /* 检查是否清空 */
            {
                uint8_t r, c2, left = 0;
                for (r = 0; r < B_ROWS; r++)
                    for (c2 = 0; c2 < B_COLS; c2++)
                        if (g_b.bricks[r][c2]) left++;

                if (left == 0)
                {
                    /* 下一关 */
                    g_b.level++;
                    breakout_reset();
                    breakout_launch();
                    return;
                }
            }
        }
    }

    /* 擦旧球, 画新球 */
    breakout_erase_ball();
    g_b.ball_x = nx;
    g_b.ball_y = ny;
    breakout_draw_ball();
}

/* ==================== 游戏结束界面 ==================== */
static void breakout_game_over_ui(void)
{
    uint16_t cx = lcddev.width / 2;
    uint16_t cy = lcddev.height / 2;
    char buf[48];

    lcd_fill(cx - 120, cy - 50, cx + 120, cy + 50, GRAYBLUE);

    lcd_show_string(cx - 96, cy - 40, 200, 16, 16, "GAME OVER", WHITE);

    sprintf(buf, "Score:%lu", (unsigned long)g_b.score);
    lcd_show_string(cx - 96, cy - 16, 200, 16, 16, buf, WHITE);

    sprintf(buf, "High :%lu", (unsigned long)g_b.high_score);
    lcd_show_string(cx - 96, cy + 8, 200, 16, 16, buf, YELLOW);

    lcd_show_string(cx - 96, cy + 30, 240, 16, 16, "Space:Restart", WHITE);
    lcd_show_string(cx - 96, cy + 50, 240, 16, 16, "Esc:Menu", WHITE);

    breakout_save_high_score();
}

/* ==================== game_api 接口实现 ==================== */

static void breakout_init(void)
{
    breakout_load_high_score();
    breakout_reset();
}

static void breakout_update(void)
{
    if (g_b.state != 0) return;

    uint32_t now = HAL_GetTick();
    if ((uint32_t)(now - g_b.tick) >= 16)   /* 约 60fps */
    {
        g_b.tick = now;
        breakout_update_ball();
    }
}

static void breakout_on_input(game_cmd_t cmd)
{
    switch (g_b.state)
    {
        case 0:   /* running */
        {
            switch (cmd)
            {
                case CMD_LEFT:
                    if (g_b.paddle_x > 0)
                    {
                        breakout_erase_paddle();
                        g_b.paddle_x -= 12;
                        if (g_b.paddle_x < 0) g_b.paddle_x = 0;
                        breakout_draw_paddle();
                        /* 球未发出时跟随挡板 */
                        if (!g_b.launched)
                        {
                            breakout_erase_ball();
                            g_b.ball_x = (g_b.paddle_x + B_PADDLE_W / 2) << 6;
                            breakout_draw_ball();
                        }
                    }
                    break;

                case CMD_RIGHT:
                    if (g_b.paddle_x + B_PADDLE_W < lcddev.width)
                    {
                        breakout_erase_paddle();
                        g_b.paddle_x += 12;
                        if (g_b.paddle_x + B_PADDLE_W > lcddev.width)
                            g_b.paddle_x = lcddev.width - B_PADDLE_W;
                        breakout_draw_paddle();
                        if (!g_b.launched)
                        {
                            breakout_erase_ball();
                            g_b.ball_x = (g_b.paddle_x + B_PADDLE_W / 2) << 6;
                            breakout_draw_ball();
                        }
                    }
                    break;

                case CMD_ACTION:
                case CMD_RESTART:
                    if (!g_b.launched)
                    {
                        breakout_launch();
                    }
                    break;

                case CMD_QUIT:
                    g_b.state = 2;
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
                breakout_reset();
            }
            else if (cmd == CMD_QUIT)
            {
                g_b.state = 2;
            }
            break;
        }

        default:
            break;
    }
}

static void breakout_exit(void)
{
    breakout_save_high_score();
}

static const game_t s_breakout_game =
{
    .name     = "Breakout",
    .init     = breakout_init,
    .update   = breakout_update,
    .render   = NULL,
    .on_input = breakout_on_input,
    .exit     = breakout_exit,
};

const game_t *breakout_get_game(void)
{
    return &s_breakout_game;
}

uint8_t breakout_is_running(void)
{
    return (g_b.state != 2);
}
