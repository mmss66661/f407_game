/**
 ****************************************************************************************************
 * @file        save.c
 * @author      (本项目: 游戏机存档模块)
 * @version     V1.0
 * @date        2026-09-24
 * @brief       SD 卡存档模块实现 —— FatFs 文件系统 + magic/checksum 校验
 ****************************************************************************************************
 */

#include "./save.h"
#include "string.h"

/* 存档文件 magic 头(4 字节), 用于识别合法存档 */
#define SAVE_MAGIC          0x53415631UL    /* "SAV1" */

/* 存档文件最大 payload 字节数(留足余量) */
#define SAVE_MAX_PAYLOAD    64

/* SD 卡盘符 */
#define SD_DRIVE            "0:"

/* 存档目录 */
#define SAVE_DIR            "0:/SAVE"

static FATFS s_fs;      /* FatFs 工作区(静态, 不用堆) */
static uint8_t s_ready = 0;   /* 存档功能是否可用 */

/* ==================== 简单校验和(逐字节累加) ==================== */
static uint32_t save_checksum(const uint8_t *data, uint32_t len)
{
    uint32_t sum = 0;
    uint32_t i;
    for (i = 0; i < len; i++)
    {
        sum = (sum << 1) ^ data[i] ^ (sum >> 31);
    }
    return sum;
}

/**
 * @brief       初始化 FatFs + 挂载 SD 卡 + 创建 /SAVE 目录
 * @retval      0: 成功; 非 0: 失败
 */
uint8_t save_init(void)
{
    FRESULT res;

    s_ready = 0;

    /* 挂载 SD 卡 */
    res = f_mount(&s_fs, SD_DRIVE, 1);   /* 1: 立即挂载 */
    if (res != FR_OK)
    {
        return 1;
    }

    /* 尝试打开 /SAVE 目录, 不存在则创建 */
    res = f_mkdir(SAVE_DIR);
    if (res != FR_OK && res != FR_EXIST)
    {
        return 2;
    }

    s_ready = 1;
    return 0;
}

/**
 * @brief       存档是否可用
 */
uint8_t save_available(void)
{
    return s_ready;
}

/**
 * @brief       通用写存档
 */
uint8_t save_write(const char *fname, const void *data, uint32_t size)
{
    FRESULT res;
    FIL fp;
    UINT bw;
    uint8_t path[48];
    uint8_t buf[SAVE_MAX_PAYLOAD + 8];   /* magic(4) + payload + checksum(4) */
    uint32_t magic = SAVE_MAGIC;
    uint32_t cksum;

    if (!s_ready || data == NULL || size > SAVE_MAX_PAYLOAD)
    {
        return 1;
    }

    /* 构造完整路径 "0:/SAVE/xxx.sav" */
    memset(buf, 0, sizeof(buf));
    strcpy((char *)path, SAVE_DIR);
    strcat((char *)path, "/");
    strcat((char *)path, fname);

    /* 组装: magic + data + checksum */
    memcpy(buf, &magic, 4);
    memcpy(buf + 4, data, size);
    cksum = save_checksum(buf, 4 + size);
    memcpy(buf + 4 + size, &cksum, 4);

    /* 写入文件 */
    res = f_open(&fp, (const TCHAR *)path, FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK)
    {
        return 2;
    }

    res = f_write(&fp, buf, 4 + size + 4, &bw);
    f_close(&fp);

    if (res != FR_OK || bw != (4 + size + 4))
    {
        return 3;
    }

    return 0;
}

/**
 * @brief       通用读存档
 */
uint8_t save_read(const char *fname, void *data, uint32_t size)
{
    FRESULT res;
    FIL fp;
    UINT br;
    uint8_t path[48];
    uint8_t buf[SAVE_MAX_PAYLOAD + 8];
    uint32_t magic;
    uint32_t cksum_read, cksum_calc;

    if (!s_ready || data == NULL || size > SAVE_MAX_PAYLOAD)
    {
        return 1;
    }

    strcpy((char *)path, SAVE_DIR);
    strcat((char *)path, "/");
    strcat((char *)path, fname);

    res = f_open(&fp, (const TCHAR *)path, FA_READ);
    if (res != FR_OK)
    {
        return 2;   /* 文件不存在 */
    }

    res = f_read(&fp, buf, 4 + size + 4, &br);
    f_close(&fp);

    if (res != FR_OK || br != (4 + size + 4))
    {
        return 3;
    }

    /* 校验 magic */
    memcpy(&magic, buf, 4);
    if (magic != SAVE_MAGIC)
    {
        return 4;   /* 不是合法存档 */
    }

    /* 校验 checksum */
    memcpy(&cksum_read, buf + 4 + size, 4);
    cksum_calc = save_checksum(buf, 4 + size);
    if (cksum_read != cksum_calc)
    {
        return 5;   /* 校验失败(数据损坏) */
    }

    memcpy(data, buf + 4, size);
    return 0;
}

/**
 * @brief       写 uint32_t 数值
 */
uint8_t save_write_u32(const char *fname, uint32_t val)
{
    return save_write(fname, &val, 4);
}

/**
 * @brief       读 uint32_t 数值
 */
uint8_t save_read_u32(const char *fname, uint32_t *val)
{
    return save_read(fname, val, 4);
}
