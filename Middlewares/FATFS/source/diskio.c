/**
 ****************************************************************************************************
 * @file        diskio.c
 * @author      (移植适配: CMake/GCC, 仅保留 SD 卡卷, 去掉 NORFLASH 依赖)
 * @version     V1.0
 * @date        2026-09-24
 * @brief       FATFS 底层(diskio) 驱动代码 —— 对接本项目 sdio_sdcard 驱动
 *
 *              本项目只使用 SD 卡(卷标 0), 去掉了原正点原子工程里的外部 SPI FLASH
 *              (NORFLASH) 分支, 避免引入不必要的依赖。
 ****************************************************************************************************
 */

#include "./BSP/SDIO/sdio_sdcard.h"
#include "./FATFS/source/diskio.h"

/* SD 卡卷标号 */
#define SD_CARD     0

/**
 * @brief       获得磁盘状态
 * @param       pdrv : 磁盘编号 0~9
 * @retval      执行结果(参见 DSTATUS 定义)
 */
DSTATUS disk_status(BYTE pdrv)
{
    (void)pdrv;
    return RES_OK;
}

/**
 * @brief       初始化磁盘
 * @param       pdrv : 磁盘编号 0~9
 * @retval      执行结果
 */
DSTATUS disk_initialize(BYTE pdrv)
{
    uint8_t res = 1;

    if (pdrv == SD_CARD)
    {
        res = sd_init();        /* SD 卡初始化 */
    }

    if (res)
    {
        return STA_NOINIT;
    }
    else
    {
        return 0;               /* 初始化成功 */
    }
}

/**
 * @brief       读扇区
 * @param       pdrv   : 磁盘编号
 * @param       buff   : 数据接收缓冲首地址
 * @param       sector : 扇区地址(LBA)
 * @param       count  : 需要读取的扇区数
 * @retval      执行结果
 */
DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    uint8_t res = 1;

    if (!count) return RES_PARERR;      /* count 不能等于 0 */

    if (pdrv == SD_CARD)
    {
        res = sd_read_disk(buff, (uint32_t)sector, count);

        while (res)                     /* 读出错则重新初始化后重试 */
        {
            sd_init();
            res = sd_read_disk(buff, (uint32_t)sector, count);
        }
    }

    return (res == 0x00) ? RES_OK : RES_ERROR;
}

/**
 * @brief       写扇区
 * @param       pdrv   : 磁盘编号
 * @param       buff   : 发送数据缓存区首地址
 * @param       sector : 扇区地址(LBA)
 * @param       count  : 需要写入的扇区数
 * @retval      执行结果
 */
DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    uint8_t res = 1;

    if (!count) return RES_PARERR;      /* count 不能等于 0 */

    if (pdrv == SD_CARD)
    {
        res = sd_write_disk((uint8_t *)buff, (uint32_t)sector, count);

        while (res)                     /* 写出错则重新初始化后重试 */
        {
            sd_init();
            res = sd_write_disk((uint8_t *)buff, (uint32_t)sector, count);
        }
    }

    return (res == 0x00) ? RES_OK : RES_ERROR;
}

/**
 * @brief       获取其他控制参数
 * @param       pdrv : 磁盘编号
 * @param       cmd  : 控制代码
 * @param       buff : 缓冲区指针
 * @retval      执行结果
 */
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    DRESULT res = RES_ERROR;

    if (pdrv == SD_CARD)
    {
        switch (cmd)
        {
            case CTRL_SYNC:
                res = RES_OK;
                break;

            case GET_SECTOR_SIZE:
                *(WORD *)buff = 512;
                res = RES_OK;
                break;

            case GET_BLOCK_SIZE:
                *(DWORD *)buff = g_sd_card_info_handle.LogBlockSize;
                res = RES_OK;
                break;

            case GET_SECTOR_COUNT:
                *(LBA_t *)buff = g_sd_card_info_handle.LogBlockNbr;
                res = RES_OK;
                break;

            default:
                res = RES_PARERR;
                break;
        }
    }
    else
    {
        res = RES_PARERR;       /* 其他卷不支持 */
    }

    return res;
}
