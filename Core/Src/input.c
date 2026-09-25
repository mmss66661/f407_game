/**
 ****************************************************************************************************
 * @file        input.c
 * @author      (本项目: 游戏机架构, 参照正点原子实验55 USB鼠标键盘例程)
 * @version     V2.0
 * @date        2026-09-24
 * @brief       统一输入层实现: USB 键盘 + 板载按键 -> 统一游戏命令
 *
 *              通过 input_set_handler 设置回调, 把命令投递给当前激活的
 *              游戏或菜单, 实现输入源与游戏逻辑的解耦。
 ****************************************************************************************************
 */

#include "./input.h"
#include "./SYSTEM/delay/delay.h"
#include "./BSP/KEY/key.h"
#include "./gamepad.h"
#include "usbh_core.h"
#include "usbh_hid.h"
#include "usbh_hid_keybd.h"

/* USB Host 句柄 */
static USBH_HandleTypeDef s_usb_host;

/* 设备类型(0=未连接, 1=键盘, 2=鼠标, 3=其他) */
static uint8_t s_dev_type = 0;

/* 当前命令回调(指向游戏或菜单的处理函数) */
static input_cmd_handler_t s_cmd_handler = NULL;

/* ============ 长按连发状态机(所有按钮统一) ============
 * 逻辑键定义: 用一个小整数索引表示"哪个逻辑按键被按下",
 * 把手柄按钮位图、十字键、摇杆、USB键盘按键统一映射到逻辑键。
 * 每个逻辑键记录: 按下时刻 tick + 上次连发 tick。
 */
#define GP_KEY_COUNT      32u          /* 逻辑键总数: 15按钮 + 4十字键 + 4摇杆 + 7键盘 + 预留 */

#define GP_HOLD_THRESHOLD_MS   500u    /* 长按阈值: 按住超过 500ms 进入连发 */
#define GP_REPEAT_INTERVAL_MS  200u    /* 连发间隔: 进入连发后每 200ms 触发一次 */

/* 逻辑键索引分配 */
#define GP_KEY_BTN_BASE     0u          /* 手柄按钮位图 bit0~14 -> 索引 0~14 */
#define GP_KEY_HAT_BASE     15u         /* 手柄十字键: 15=左 16=右 17=上 18=下 */
#define GP_KEY_STICK_BASE   19u         /* 手柄摇杆:   19=左 20=右 21=上 22=下 */
#define KBD_KEY_BASE        23u         /* USB键盘: 23=左 24=右 25=上 26=下 27=动作 28=重开 29=退出 */

static uint32_t s_gp_key_press_tick[GP_KEY_COUNT]; /* 每个逻辑键按下时刻(0=未按) */
static uint32_t s_gp_key_repeat_tick[GP_KEY_COUNT];/* 每个逻辑键上次连发时刻 */
static uint8_t  s_gp_key_held[GP_KEY_COUNT];       /* 每个逻辑键当前是否按住 */

/* 前置声明(实现位于文件后部) */
static uint8_t gp_key_update(uint8_t key, uint8_t pressed_now);
static game_cmd_t gp_key_to_cmd(uint8_t key);

/* ==================== 投递命令(安全封装) ==================== */
static void input_dispatch(game_cmd_t cmd)
{
    if (s_cmd_handler != NULL)
    {
        s_cmd_handler(cmd);
    }
}

/* ==================== USB Host 用户状态回调 ==================== */
static void input_usb_user_process(USBH_HandleTypeDef *phost, uint8_t id)
{
    (void)phost;

    switch (id)
    {
        case HOST_USER_DISCONNECTION:
            s_dev_type = 0;
            break;

        case HOST_USER_CLASS_ACTIVE:
        {
            uint8_t proto = phost->device.CfgDesc.Itf_Desc[phost->device.current_interface].bInterfaceProtocol;
            if (proto == HID_KEYBRD_BOOT_CODE)      s_dev_type = 1;
            else if (proto == HID_MOUSE_BOOT_CODE)  s_dev_type = 2;
            else                                    s_dev_type = 3;
            break;
        }

        case HOST_USER_CONNECTION:
        default:
            break;
    }
}

/* ==================== USB 枚举死机检测 ==================== */
static uint8_t input_usb_check_dead(USBH_HandleTypeDef *phost)
{
    static uint16_t errcnt = 0;

    if (phost->gState == HOST_ENUMERATION &&
        (phost->EnumState == ENUM_IDLE || phost->EnumState == ENUM_GET_FULL_DEV_DESC))
    {
        errcnt++;
        if (errcnt > 2000)
        {
            errcnt = 0;
            return 1;
        }
    }
    else
    {
        errcnt = 0;
    }

    return 0;
}

/* ==================== USB Host 重新连接 ==================== */
static void input_usb_reconnect(void)
{
    HID_Class.DeInit(&s_usb_host);
    USBH_DeInit(&s_usb_host);

    RCC->AHB2RSTR |= 1 << 7;
    delay_ms(5);
    RCC->AHB2RSTR &= ~(1 << 7);

    memset(&s_usb_host, 0, sizeof(s_usb_host));

    USBH_Init(&s_usb_host, input_usb_user_process, 0);
    USBH_RegisterClass(&s_usb_host, USBH_HID_CLASS);
    USBH_Start(&s_usb_host);

    s_dev_type = 0;
}

/* ==================== USB 键盘初始化 ==================== */
void input_usb_init(void)
{
    USBH_Init(&s_usb_host, input_usb_user_process, 0);
    USBH_RegisterClass(&s_usb_host, USBH_HID_CLASS);
    USBH_Start(&s_usb_host);
}

/* ==================== 设置命令回调 ==================== */
void input_set_handler(input_cmd_handler_t handler)
{
    s_cmd_handler = handler;
}

/* ==================== 板载按键 -> 命令 ==================== */
static void input_process_keys(void)
{
    uint8_t key = key_scan(0);

    switch (key)
    {
        case KEY0_PRES:  input_dispatch(CMD_LEFT);   break;
        case KEY1_PRES:  input_dispatch(CMD_DOWN);   break;
        case KEY2_PRES:  input_dispatch(CMD_UP);     break;
        case WKUP_PRES:  input_dispatch(CMD_RIGHT);  break;
        default: break;
    }
}

/* ==================== USB 键盘 -> 命令 ==================== */
static void input_process_usb(void)
{
    HID_KEYBD_Info_TypeDef *info;
    uint8_t left = 0, right = 0, up = 0, down = 0;
    uint8_t act = 0, restart = 0, quit = 0;
    uint8_t i;

    if (s_usb_host.gState != HOST_CLASS || s_dev_type != 1)
    {
        /* 键盘断开: 清空所有键盘逻辑键状态 */
        for (i = 0; i < 7; i++)
        {
            s_gp_key_held[KBD_KEY_BASE + i] = 0;
            s_gp_key_press_tick[KBD_KEY_BASE + i] = 0;
            s_gp_key_repeat_tick[KBD_KEY_BASE + i] = 0;
        }
        return;
    }

    if (USBH_HID_GetDeviceType(&s_usb_host) != HID_KEYBOARD)
    {
        for (i = 0; i < 7; i++)
        {
            s_gp_key_held[KBD_KEY_BASE + i] = 0;
            s_gp_key_press_tick[KBD_KEY_BASE + i] = 0;
            s_gp_key_repeat_tick[KBD_KEY_BASE + i] = 0;
        }
        return;
    }

    info = USBH_HID_GetKeybdInfo(&s_usb_host);
    if (info == NULL)
    {
        return;
    }

    /* 扫描当前按下的键, 归类到 7 个逻辑键 */
    for (i = 0; i < 6; i++)
    {
        uint8_t k = info->keys[i];
        if (k == 0) continue;

        switch (k)
        {
            case KEY_LEFTARROW:  case KEY_A: left = 1;     break;
            case KEY_RIGHTARROW: case KEY_D: right = 1;    break;
            case KEY_UPARROW:    case KEY_W: up = 1;       break;
            case KEY_DOWNARROW:  case KEY_S: down = 1;     break;
            case KEY_SPACEBAR:   act = 1;                  break;
            case KEY_ENTER:      restart = 1;              break;
            case KEY_ESCAPE:     quit = 1;                 break;
            default: break;
        }
    }

    /* 用统一长按连发状态机处理 7 个键盘逻辑键 */
    if (gp_key_update(KBD_KEY_BASE + 0, left))    input_dispatch(CMD_LEFT);
    if (gp_key_update(KBD_KEY_BASE + 1, right))   input_dispatch(CMD_RIGHT);
    if (gp_key_update(KBD_KEY_BASE + 2, up))      input_dispatch(CMD_UP);
    if (gp_key_update(KBD_KEY_BASE + 3, down))    input_dispatch(CMD_DOWN);
    if (gp_key_update(KBD_KEY_BASE + 4, act))     input_dispatch(CMD_ACTION);
    if (gp_key_update(KBD_KEY_BASE + 5, restart)) input_dispatch(CMD_RESTART);
    if (gp_key_update(KBD_KEY_BASE + 6, quit))    input_dispatch(CMD_QUIT);
}

/* ==================== 手柄 -> 命令 ==================== */

/* 逻辑键 -> 命令 的映射 */
static game_cmd_t gp_key_to_cmd(uint8_t key)
{
    if (key < GP_KEY_HAT_BASE)      /* 按钮位图 */
    {
        switch (key)
        {
            case GP_BTN_A:      return CMD_ACTION;
            case GP_BTN_B:      return CMD_RESTART;
            case GP_BTN_START:  return CMD_RESTART;
            case GP_BTN_BACK:   return CMD_QUIT;
            case GP_BTN_UP:     return CMD_UP;
            case GP_BTN_DOWN:   return CMD_DOWN;
            case GP_BTN_LEFT:   return CMD_LEFT;
            case GP_BTN_RIGHT:  return CMD_RIGHT;
            default:            return CMD_NONE;
        }
    }
    else if (key < GP_KEY_STICK_BASE)  /* 十字键 */
    {
        switch (key - GP_KEY_HAT_BASE)
        {
            case 0: return CMD_LEFT;   /* 左 */
            case 1: return CMD_RIGHT;  /* 右 */
            case 2: return CMD_UP;     /* 上 */
            case 3: return CMD_DOWN;   /* 下 */
            default: return CMD_NONE;
        }
    }
    else if (key < KBD_KEY_BASE)       /* 摇杆 */
    {
        switch (key - GP_KEY_STICK_BASE)
        {
            case 0: return CMD_LEFT;
            case 1: return CMD_RIGHT;
            case 2: return CMD_UP;
            case 3: return CMD_DOWN;
            default: return CMD_NONE;
        }
    }
    else                                /* USB 键盘 */
    {
        switch (key - KBD_KEY_BASE)
        {
            case 0: return CMD_LEFT;    /* 左 */
            case 1: return CMD_RIGHT;   /* 右 */
            case 2: return CMD_UP;      /* 上 */
            case 3: return CMD_DOWN;    /* 下 */
            case 4: return CMD_ACTION;  /* 动作(空格) */
            case 5: return CMD_RESTART; /* 重开(回车) */
            case 6: return CMD_QUIT;    /* 退出(ESC) */
            default: return CMD_NONE;
        }
    }
}

/* 处理一个逻辑键的按下状态: 输入当前是否按住(pressed_now)
 * 返回是否应该投递命令(边沿 or 长按连发) */
static uint8_t gp_key_update(uint8_t key, uint8_t pressed_now)
{
    uint32_t now = HAL_GetTick();
    uint8_t was_held = s_gp_key_held[key];

    /* --- 松开处理: 清零状态 --- */
    if (!pressed_now)
    {
        if (was_held)
        {
            s_gp_key_held[key]     = 0;
            s_gp_key_press_tick[key] = 0;
            s_gp_key_repeat_tick[key] = 0;
        }
        return 0;
    }

    /* --- 按下处理 --- */
    if (!was_held)
    {
        /* 刚按下: 记录时刻, 立即触发一次(边沿) */
        s_gp_key_held[key]      = 1;
        s_gp_key_press_tick[key] = now;
        s_gp_key_repeat_tick[key] = now;
        return 1;
    }

    /* 一直按住: 判断是否进入长按连发 */
    uint32_t held_ms = now - s_gp_key_press_tick[key];
    if (held_ms >= GP_HOLD_THRESHOLD_MS)
    {
        /* 超过长按阈值, 且距上次连发 >= 间隔, 触发一次连发 */
        if (now - s_gp_key_repeat_tick[key] >= GP_REPEAT_INTERVAL_MS)
        {
            s_gp_key_repeat_tick[key] = now;
            return 1;
        }
    }
    return 0;
}

static void input_process_gamepad(void)
{
    uint16_t btn = g_gamepad.buttons;
    int8_t hat_x = g_gamepad.hat_x;
    int8_t hat_y = g_gamepad.hat_y;
    int8_t stick_x, stick_y;
    uint8_t bit;

    /* 手柄未在线则无输入, 清空所有按键状态 */
    if (!gamepad_online())
    {
        memset(s_gp_key_held, 0, sizeof(s_gp_key_held));
        memset(s_gp_key_press_tick, 0, sizeof(s_gp_key_press_tick));
        memset(s_gp_key_repeat_tick, 0, sizeof(s_gp_key_repeat_tick));
        return;
    }

    /* 摇杆方向量化(死区外 -> ±1) */
    stick_x = (g_gamepad.lx < -GP_STICK_DEADZONE) ? -1 :
              (g_gamepad.lx >  GP_STICK_DEADZONE) ?  1 : 0;
    stick_y = (g_gamepad.ly < -GP_STICK_DEADZONE) ? -1 :
              (g_gamepad.ly >  GP_STICK_DEADZONE) ?  1 : 0;

    /* ---- 按钮位图: 逐位处理(支持长按连发) ---- */
    for (bit = 0; bit < 15; bit++)
    {
        uint8_t pressed_now = (btn >> bit) & 1u;
        if (gp_key_update(bit, pressed_now))
        {
            game_cmd_t c = gp_key_to_cmd(bit);
            if (c != CMD_NONE) input_dispatch(c);
        }
    }

    /* ---- 十字键: 4 方向(支持长按连发) ---- */
    {
        uint8_t l = (hat_x < 0);
        uint8_t r = (hat_x > 0);
        uint8_t u = (hat_y < 0);
        uint8_t d = (hat_y > 0);
        if (gp_key_update(GP_KEY_HAT_BASE + 0, l)) input_dispatch(CMD_LEFT);
        if (gp_key_update(GP_KEY_HAT_BASE + 1, r)) input_dispatch(CMD_RIGHT);
        if (gp_key_update(GP_KEY_HAT_BASE + 2, u)) input_dispatch(CMD_UP);
        if (gp_key_update(GP_KEY_HAT_BASE + 3, d)) input_dispatch(CMD_DOWN);
    }

    /* ---- 摇杆: 4 方向(支持长按连发) ---- */
    {
        uint8_t l = (stick_x < 0);
        uint8_t r = (stick_x > 0);
        uint8_t u = (stick_y < 0);
        uint8_t d = (stick_y > 0);
        if (gp_key_update(GP_KEY_STICK_BASE + 0, l)) input_dispatch(CMD_LEFT);
        if (gp_key_update(GP_KEY_STICK_BASE + 1, r)) input_dispatch(CMD_RIGHT);
        if (gp_key_update(GP_KEY_STICK_BASE + 2, u)) input_dispatch(CMD_UP);
        if (gp_key_update(GP_KEY_STICK_BASE + 3, d)) input_dispatch(CMD_DOWN);
    }
}

/* ==================== 输入处理(一帧一次) ==================== */
void input_process(void)
{
    USBH_Process(&s_usb_host);

    if (input_usb_check_dead(&s_usb_host))
    {
        input_usb_reconnect();
        return;
    }

    input_process_usb();
    input_process_keys();
    input_process_gamepad();
}
