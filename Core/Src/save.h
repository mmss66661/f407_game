/**
 ****************************************************************************************************
 * @file        save.h
 * @author      (本项目: 游戏机存档模块)
 * @version     V1.0
 * @date        2026-09-24
 * @brief       SD 卡存档模块 —— 基于 FatFs 文件系统持久化游戏数据
 *
 *              每个游戏用一个独立存档文件, 存到 SD 卡 /SAVE/ 目录:
 *                /SAVE/snake.sav   贪吃蛇最高分
 *                /SAVE/tetris.sav  俄罗斯方块最高分
 *
 *              存档文件格式(带 magic + 校验和, 防止损坏/脏数据):
 *                [magic 4字节][payload 数据][checksum 4字节]
 *
 *              对外提供通用的读写接口, 游戏只需指定文件名和结构体即可。
 ****************************************************************************************************
 */

#ifndef __SAVE_H
#define __SAVE_H

#include "./SYSTEM/sys/sys.h"
#include "./FATFS/source/ff.h"

/* ==================== 存档管理 ==================== */

/* 初始化 FatFs + 挂载 SD 卡 + 创建 /SAVE 目录
 * @retval  0: 成功; 非 0: 失败(无 SD 卡/挂载失败, 存档功能禁用) */
uint8_t save_init(void);

/* 通用: 把任意结构体数据写入存档文件
 * @param  fname  文件名(如 "snake.sav")
 * @param  data   要保存的数据首地址
 * @param  size   数据字节数(<= 存档文件最大 payload)
 * @retval 0: 成功; 非 0: 失败 */
uint8_t save_write(const char *fname, const void *data, uint32_t size);

/* 通用: 从存档文件读取结构体数据
 * @param  fname  文件名
 * @param  data   读取到的数据存放地址
 * @param  size   期望读取的字节数
 * @retval 0: 成功; 非 0: 失败(文件不存在/损坏/校验失败) */
uint8_t save_read(const char *fname, void *data, uint32_t size);

/* 存档是否可用(初始化成功) */
uint8_t save_available(void);

/* 方便: 读写 uint32_t 数值(如最高分) */
uint8_t save_write_u32(const char *fname, uint32_t val);
uint8_t save_read_u32(const char *fname, uint32_t *val);

#endif /* __SAVE_H */
