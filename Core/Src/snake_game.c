/**
 ****************************************************************************************************
 * @file        snake_game.c
 * @author      (本项目: 游戏机架构)
 * @version     V2.0
 * @date        2026-09-24
 * @brief       贪吃蛇游戏实现 —— 符合统一游戏接口(game_api)
 *
 *              游戏玩法:
 *                1. 蛇在网格地图上移动, 吃到食物(黄色方块)身体变长 +1 分
 *                2. 撞墙或撞到自己身体 -> 游戏结束
 *                3. 每 +5 分速度提升一档, 难度递增
 *                4. 最高分通过 save 模块持久化到 SD 卡 /SAVE/snake.sav
 *
 *              技术要点:
 *                - 蛇身用定长数组实现, 避免动态内存
 *                - 食物用 SysTick 计数值做伪随机源(裸机无 rand 依赖)
 *                - 非阻塞计时(HAL_GetTick 绝对时间戳), 保证操控流畅
 ****************************************************************************************************
 */

#include "./snake_game.h"
#include "./save.h"
#include "string.h"
#include "stdio.h"

/* ==================== 地图/网格参数 ==================== */
/* 注意: 探索者 F407 的 4.3 寸屏实际显示区域高度可能为 480(横屏)而非 800,
 * 故地图尺寸需控制在高度 480 以内, 兼容竖屏/横屏两种布局。 */
#define CELL_SIZE        12          /* 每个格子的像素边长 */
#define MAP_COLS         30          /* 地图列数(12*30=360, 宽480留边) */
#define MAP_ROWS         28          /* 地图行数(12*28=336) */

/* 游戏区左上角偏移(自适应屏幕) */
#define MAP_OFF_X        ((lcddev.width  - MAP_COLS * CELL_SIZE) / 2)
#define MAP_OFF_Y        90           /* 标题+分数区下方(336+90=426 < 480) */

/* 蛇最大长度 */
#define SNAKE_MAX_LEN    (MAP_COLS * MAP_ROWS)

/* 方向 */
#define DIR_UP    0
#define DIR_DOWN  1
#define DIR_LEFT  2
#define DIR_RIGHT 3

/* 存档文件名 */
#define SNAKE_SAVE_FILE  "snake.sav"

/* ==================== 游戏状态变量 ==================== */
typedef struct
{
    uint16_t sx[SNAKE_MAX_LEN];   /* 蛇身网格坐标, [0]=蛇头 */
    uint16_t sy[SNAKE_MAX_LEN];
    uint16_t len;

    uint8_t  dir;
    uint8_t  next_dir;

    uint16_t food_x, food_y;

    uint32_t score;
    uint32_t high_score;

    uint32_t tick;                /* 上次移动时间戳(非阻塞计时) */
    uint32_t speed;               /* 移动周期 ms */

    snake_state_t state;
} snake_t;

static snake_t g_snake;

/* ==================== 内部函数声明 ==================== */
static void snake_reset(void);
static void snake_draw_cell(uint16_t gx, uint16_t gy, uint16_t color);
static void snake_spawn_food(void);
static void snake_move(void);
static void snake_draw_all(void);
static void snake_draw_score(void);
static void snake_game_over_ui(void);
static void snake_load_high_score(void);
static void snake_save_high_score(void);
static uint32_t snake_rand(void);

/* ==================== 伪随机数 ==================== */
static uint32_t snake_rand_seed;

/* 播种: 用运行时间戳保证每次(含首次)种子非零且不同, 避免食物位置固定 */
static void snake_seed(void)
{
    snake_rand_seed = HAL_GetTick() ^ (uint32_t)(SysTick->VAL) ^ 0x9E3779B9UL;
    if (snake_rand_seed == 0) snake_rand_seed = 1;
}

static uint32_t snake_rand(void)
{
    snake_rand_seed = snake_rand_seed * 1103515245UL + 12345UL;
    return (snake_rand_seed >> 16) & 0x7FFF;
}

/* ==================== 读取最高分(save 模块) ==================== */
static void snake_load_high_score(void)
{
    g_snake.high_score = 0;

    if (save_available())
    {
        save_read_u32(SNAKE_SAVE_FILE, &g_snake.high_score);
    }
}

/* ==================== 保存最高分(save 模块) ==================== */
static void snake_save_high_score(void)
{
    if (save_available())
    {
        save_write_u32(SNAKE_SAVE_FILE, g_snake.high_score);
    }
}

/* ==================== 重置游戏 ==================== */
static void snake_reset(void)
{
    uint16_t cx = MAP_COLS / 2;
    uint16_t cy = MAP_ROWS / 2;
    uint8_t i;

    g_snake.len = 4;
    for (i = 0; i < g_snake.len; i++)
    {
        g_snake.sx[i] = cx - i;
        g_snake.sy[i] = cy;
    }
    g_snake.dir = DIR_RIGHT;
    g_snake.next_dir = DIR_RIGHT;
    g_snake.score = 0;
    g_snake.speed = 200;
    g_snake.tick = HAL_GetTick();
    g_snake.state = SNAKE_RUNNING;

    snake_seed();           /* 每次重置重新播种, 保证食物位置随机 */
    snake_spawn_food();

    lcd_clear(WHITE);
    snake_draw_all();
    snake_draw_score();
}

/* ==================== 生成食物 ==================== */
static void snake_spawn_food(void)
{
    uint16_t gx, gy;
    uint8_t overlap;
    uint16_t i;
    uint16_t guard = 0;

    do
    {
        overlap = 0;
        gx = snake_rand() % MAP_COLS;
        gy = snake_rand() % MAP_ROWS;

        for (i = 0; i < g_snake.len; i++)
        {
            if (g_snake.sx[i] == gx && g_snake.sy[i] == gy)
            {
                overlap = 1;
                break;
            }
        }

        if (++guard > 1000) break;
    }
    while (overlap);

    /* 兜底: 若上面仍与蛇身重叠(随机数极端情况), 顺序扫描找第一个空格子,
     * 保证食物必定可见, 不会被蛇身遮挡。 */
    if (overlap)
    {
        for (gy = 0; gy < MAP_ROWS; gy++)
        {
            for (gx = 0; gx < MAP_COLS; gx++)
            {
                uint8_t occupied = 0;
                for (i = 0; i < g_snake.len; i++)
                {
                    if (g_snake.sx[i] == gx && g_snake.sy[i] == gy)
                    {
                        occupied = 1;
                        break;
                    }
                }
                if (!occupied)
                {
                    g_snake.food_x = gx;
                    g_snake.food_y = gy;
                    return;
                }
            }
        }
        /* 理论上蛇未占满整张地图, 一定能找到空格; 极端满屏则放 (0,0) */
        g_snake.food_x = 0;
        g_snake.food_y = 0;
        return;
    }

    g_snake.food_x = gx;
    g_snake.food_y = gy;
}

/* ==================== 绘制单格 ==================== */
static void snake_draw_cell(uint16_t gx, uint16_t gy, uint16_t color)
{
    uint16_t px = MAP_OFF_X + gx * CELL_SIZE;
    uint16_t py = MAP_OFF_Y + gy * CELL_SIZE;
    lcd_fill(px, py, px + CELL_SIZE - 1, py + CELL_SIZE - 1, color);
}

/* ==================== 蛇移动一步 ==================== */
static void snake_move(void)
{
    uint16_t nx, ny;
    uint16_t old_tail_x, old_tail_y;
    uint16_t i;
    uint8_t eat;
    uint16_t check_len;

    g_snake.dir = g_snake.next_dir;

    nx = g_snake.sx[0];
    ny = g_snake.sy[0];

    switch (g_snake.dir)
    {
        case DIR_UP:    ny--; break;
        case DIR_DOWN:  ny++; break;
        case DIR_LEFT:  nx--; break;
        case DIR_RIGHT: nx++; break;
        default: break;
    }

    /* 撞墙 */
    if (nx >= MAP_COLS || ny >= MAP_ROWS)
    {
        g_snake.state = SNAKE_OVER;
        snake_game_over_ui();
        return;
    }

    /* 撞自己 */
    eat = (nx == g_snake.food_x && ny == g_snake.food_y);
    check_len = eat ? g_snake.len : g_snake.len - 1;

    for (i = 0; i < check_len; i++)
    {
        if (g_snake.sx[i] == nx && g_snake.sy[i] == ny)
        {
            g_snake.state = SNAKE_OVER;
            snake_game_over_ui();
            return;
        }
    }

    old_tail_x = g_snake.sx[g_snake.len - 1];
    old_tail_y = g_snake.sy[g_snake.len - 1];

    for (i = g_snake.len - 1; i > 0; i--)
    {
        g_snake.sx[i] = g_snake.sx[i - 1];
        g_snake.sy[i] = g_snake.sy[i - 1];
    }
    g_snake.sx[0] = nx;
    g_snake.sy[0] = ny;

    if (eat)
    {
        g_snake.sx[g_snake.len] = old_tail_x;
        g_snake.sy[g_snake.len] = old_tail_y;
        g_snake.len++;

        g_snake.score++;

        if ((g_snake.score % 5) == 0 && g_snake.speed > 60)
        {
            g_snake.speed -= 20;
        }

        if (g_snake.score > g_snake.high_score)
        {
            g_snake.high_score = g_snake.score;
        }

        snake_spawn_food();
        snake_draw_cell(g_snake.food_x, g_snake.food_y, YELLOW);
        snake_draw_score();
    }
    else
    {
        snake_draw_cell(old_tail_x, old_tail_y, WHITE);
    }

    snake_draw_cell(g_snake.sx[0], g_snake.sy[0], GREEN);
    if (g_snake.len > 1)
    {
        snake_draw_cell(g_snake.sx[1], g_snake.sy[1], LIGHTGREEN);
    }
}

/* ==================== 绘制全部画面 ==================== */
static void snake_draw_all(void)
{
    uint16_t i;

    lcd_show_string(MAP_OFF_X, 4, lcddev.width, 16, 16, "SNAKE GAME", RED);

    lcd_draw_rectangle(MAP_OFF_X - 1, MAP_OFF_Y - 1,
                       MAP_OFF_X + MAP_COLS * CELL_SIZE,
                       MAP_OFF_Y + MAP_ROWS * CELL_SIZE, BLACK);

    snake_draw_cell(g_snake.food_x, g_snake.food_y, YELLOW);

    for (i = 0; i < g_snake.len; i++)
    {
        if (i == 0)      snake_draw_cell(g_snake.sx[i], g_snake.sy[i], GREEN);
        else if (i == 1) snake_draw_cell(g_snake.sx[i], g_snake.sy[i], LIGHTGREEN);
        else             snake_draw_cell(g_snake.sx[i], g_snake.sy[i], BLUE);
    }
}

/* ==================== 绘制得分 ==================== */
static void snake_draw_score(void)
{
    char buf[48];
    uint16_t y0 = 30;

    sprintf(buf, "SCORE: %lu", (unsigned long)g_snake.score);
    lcd_show_string(MAP_OFF_X, y0, lcddev.width, 16, 16, buf, BLUE);

    sprintf(buf, "HIGH : %lu", (unsigned long)g_snake.high_score);
    lcd_show_string(MAP_OFF_X, y0 + 20, lcddev.width, 16, 16, buf, MAGENTA);

    sprintf(buf, "SPD  : %lu ms", (unsigned long)g_snake.speed);
    lcd_show_string(MAP_OFF_X, y0 + 40, lcddev.width, 16, 16, buf, GRAY);
}

/* ==================== 游戏结束界面 ==================== */
static void snake_game_over_ui(void)
{
    uint16_t cx = lcddev.width / 2;
    uint16_t cy = lcddev.height / 2;
    char buf[48];

    lcd_fill(cx - 120, cy - 50, cx + 120, cy + 50, GRAYBLUE);

    lcd_show_string(cx - 96, cy - 40, 200, 16, 16, "GAME OVER", WHITE);

    sprintf(buf, "Score:%lu", (unsigned long)g_snake.score);
    lcd_show_string(cx - 96, cy - 16, 200, 16, 16, buf, WHITE);

    sprintf(buf, "High :%lu", (unsigned long)g_snake.high_score);
    lcd_show_string(cx - 96, cy + 8, 200, 16, 16, buf, YELLOW);

    lcd_show_string(cx - 96, cy + 30, 240, 16, 16, "Space:Restart", WHITE);
    lcd_show_string(cx - 96, cy + 50, 240, 16, 16, "Esc:Menu", WHITE);

    /* 游戏结束时统一保存最高分(此时无移动, 写盘不影响游戏) */
    snake_save_high_score();
}

/* ==================== game_api 接口实现 ==================== */

static void snake_init(void)
{
    snake_load_high_score();
    snake_reset();
}

static void snake_update(void)
{
    if (g_snake.state == SNAKE_RUNNING)
    {
        uint32_t now = HAL_GetTick();
        if ((uint32_t)(now - g_snake.tick) >= g_snake.speed)
        {
            g_snake.tick = now;
            snake_move();
        }
    }
}

static void snake_on_input(game_cmd_t cmd)
{
    switch (g_snake.state)
    {
        case SNAKE_RUNNING:
            switch (cmd)
            {
                case CMD_LEFT:  if (g_snake.dir != DIR_RIGHT) g_snake.next_dir = DIR_LEFT;  break;
                case CMD_RIGHT: if (g_snake.dir != DIR_LEFT)  g_snake.next_dir = DIR_RIGHT; break;
                case CMD_UP:    if (g_snake.dir != DIR_DOWN)  g_snake.next_dir = DIR_UP;    break;
                case CMD_DOWN:  if (g_snake.dir != DIR_UP)    g_snake.next_dir = DIR_DOWN;  break;
                case CMD_QUIT:  g_snake.state = SNAKE_QUIT;   break;
                default: break;
            }
            break;

        case SNAKE_OVER:
            if (cmd == CMD_RESTART)
            {
                snake_reset();
            }
            else if (cmd == CMD_QUIT)
            {
                g_snake.state = SNAKE_QUIT;
            }
            break;

        default:
            break;
    }
}

static void snake_exit(void)
{
    /* 退出时若游戏结束未存, 再存一次确保最高分落盘 */
    snake_save_high_score();
}

static const game_t s_snake_game =
{
    .name     = "Snake",
    .init     = snake_init,
    .update   = snake_update,
    .render   = NULL,            /* 贪吃蛇用增量渲染, 无需整帧重绘 */
    .on_input = snake_on_input,
    .exit     = snake_exit,
};

const game_t *snake_get_game(void)
{
    return &s_snake_game;
}

uint8_t snake_is_running(void)
{
    return (g_snake.state != SNAKE_QUIT);
}
