/**
 ****************************************************************************************************
 * @file        main.c
 * @author      正点原子团队(ALIENTEK) / 本项目: 游戏机架构
 * @version     V2.0
 * @date        2026-09-24
 * @brief       STM32F407 游戏机 (Game Console) 主程序
 *
 *              架构:
 *                开机 -> 初始化硬件 -> FatFs 挂载 SD 卡 -> 进入游戏菜单
 *                -> 选卡启动游戏 -> 游戏运行 -> 退出返回菜单
 *
 *              SD 卡作为"卡带": 存储各游戏存档 (位于 SAVE 目录)。
 *
 *              实验平台: 正点原子 探索者 F407 开发板
 *              时钟: 168MHz
 *              外设: LCD(FSMC_NE4) + 按键(PE2/PE3/PE4/PA0) + SD卡(SDIO) + USB键盘(Host)
 ****************************************************************************************************
 */

#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include "./BSP/LED/led.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/KEY/key.h"
#include "./BSP/SRAM/sram.h"
#include "./BSP/SDIO/sdio_sdcard.h"
#include "./MALLOC/malloc.h"
#include "./game_api.h"
#include "./input.h"
#include "./save.h"
#include "./menu.h"
#include "./snake_game.h"
#include "./tetris.h"
#include "./breakout.h"
#include "./game2048.h"
#include "./gamepad.h"

int main(void)
{
    uint8_t t = 0;
    const game_t *cur_game = NULL;   /* 当前运行的游戏 */

    HAL_Init();                             /* 初始化 HAL 库 */
    sys_stm32_clock_init(336, 8, 2, 7);     /* 设置时钟 168MHz */
    delay_init(168);                        /* 延时初始化 */
    usart_init(115200);                     /* 串口初始化 */
    led_init();                             /* 初始化 LED */
    lcd_init();                             /* 初始化 LCD */
    key_init();                             /* 初始化按键 */
    sram_init();                            /* 初始化外部 SRAM */

    my_mem_init(SRAMIN);                    /* 初始化内部 SRAM 内存池 */
    my_mem_init(SRAMEX);                    /* 初始化外部 SRAM 内存池 */
    my_mem_init(SRAMCCM);                   /* 初始化 CCM 内存池 */

    /* 初始化 SD 卡 + FatFs 文件系统(游戏存档用) */
    lcd_show_string(30, 50, 240, 16, 16, "STM32 GAME CONSOLE", RED);
    lcd_show_string(30, 70, 240, 16, 16, "Mount SD Card...", BLUE);

    if (save_init() == 0)
    {
        lcd_show_string(30, 110, 240, 16, 16, "SD Card + FatFs OK", BLUE);
        lcd_show_string(30, 130, 240, 16, 16, "Save: /SAVE/*.sav", BLUE);
    }
    else
    {
        lcd_show_string(30, 110, 240, 16, 16, "SD Card Error     ", RED);
        lcd_show_string(30, 130, 240, 16, 16, "No Save Support   ", RED);
    }

    lcd_show_string(30, 170, 240, 16, 16, "Init USB Host...", BLUE);
    input_usb_init();                       /* 初始化 USB Host(键盘) */

    gamepad_init();                         /* 初始化 USART3(HC-05 手柄) */

    delay_ms(1200);

    /* 进入游戏菜单 */
    menu_init();
    menu_attach_input();

    while (1)
    {
        input_process();    /* 采集输入(USB 键盘 + 板载按键) */

        if (cur_game == NULL)
        {
            /* 在菜单: 轮询是否选中游戏 */
            cur_game = menu_loop();

            if (cur_game != NULL)
            {
                /* 启动游戏: 切换输入回调到游戏, 调用游戏 init */
                input_set_handler(cur_game->on_input);
                cur_game->init();
            }
        }
        else
        {
            /* 游戏运行中 */
            cur_game->update();   /* 逻辑更新 */
            if (cur_game->render) cur_game->render();

            /* 判断游戏是否已退出 */
            if (cur_game == snake_get_game())
            {
                if (!snake_is_running())
                {
                    cur_game->exit();
                    cur_game = NULL;
                    menu_init();
                    menu_attach_input();
                }
            }
            else if (cur_game == tetris_get_game())
            {
                if (!tetris_is_running())
                {
                    cur_game->exit();
                    cur_game = NULL;
                    menu_init();
                    menu_attach_input();
                }
            }
            else if (cur_game == breakout_get_game())
            {
                if (!breakout_is_running())
                {
                    cur_game->exit();
                    cur_game = NULL;
                    menu_init();
                    menu_attach_input();
                }
            }
            else if (cur_game == game2048_get_game())
            {
                if (!game2048_is_running())
                {
                    cur_game->exit();
                    cur_game = NULL;
                    menu_init();
                    menu_attach_input();
                }
            }
        }

        /* LED 呼吸提示 */
        t++;
        if (t >= 50)
        {
            t = 0;
            LED0_TOGGLE();
        }
    }
}

/**
  * @brief  This function is executed in case of error occurrence.
  */
void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif /* USE_FULL_ASSERT */
