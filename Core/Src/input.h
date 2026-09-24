/**
 ****************************************************************************************************
 * @file        input.h
 * @author      (本项目: 游戏机架构)
 * @version     V2.0
 * @date        2026-09-24
 * @brief       统一输入层: USB 键盘 + 板载按键 -> 统一游戏命令
 *
 *              将不同输入源(USB Host HID 键盘、板载 KEY0~2/WK_UP 独立按键)
 *              统一转换为 game_cmd_t 命令, 通过回调投递给"当前激活的游戏"。
 *
 *              键盘映射:
 *                 ↑/↓/←/→ 或 W/S/A/D   -> 上/下/左/右
 *                 空格(Space)          -> 动作(俄罗斯方块旋转)
 *                 回车(Enter)          -> 重开/确认
 *                 ESC                  -> 退出游戏, 返回菜单
 *
 *              板载按键映射:
 *                 KEY2 (PE2) -> 上
 *                 KEY1 (PE3) -> 下
 *                 KEY0 (PE4) -> 左
 *                 WK_UP(PA0) -> 右
 ****************************************************************************************************
 */

#ifndef __INPUT_H
#define __INPUT_H

#include "./SYSTEM/sys/sys.h"
#include "./game_api.h"

/* 输入命令回调: 游戏机设置此回调后, 输入层把命令投递给它 */
typedef void (*input_cmd_handler_t)(game_cmd_t cmd);

/* USB 键盘初始化(在系统/USB Host 初始化后调用) */
void input_usb_init(void);

/* 设置当前接收输入命令的回调(菜单或游戏) */
void input_set_handler(input_cmd_handler_t handler);

/* 输入处理: 一帧调用一次, 轮询 USB 键盘 + 板载按键并投递命令 */
void input_process(void);

#endif /* __INPUT_H */
