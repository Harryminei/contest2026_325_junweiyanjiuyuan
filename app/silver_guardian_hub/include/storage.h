/****************************************************************************
 * Silver Guardian Hub - 本地持久化
 *
 * 落盘位置：/data/silver_guardian/  （板子上的可写分区）
 *   rcS.nsh 里有一句 `mount -t yaffs /dev/usrdata /data`，
 *   defconfig 也开了 CONFIG_FS_YAFFS，所以 /data 是掉电不丢的。
 *
 * 写文件用"临时文件 + rename"两步，避免掉电时留下半截文件。
 * 所有接口在持久化不可用时都退化为"只在内存里成功"，不会让上层崩掉，
 * 但 storage_available() 会返回 false，关于页会把状态显示出来。
 ****************************************************************************/

#ifndef __APP_SILVER_GUARDIAN_HUB_INCLUDE_STORAGE_H
#define __APP_SILVER_GUARDIAN_HUB_INCLUDE_STORAGE_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * @brief 初始化持久化（创建目录、做一次可写性探测）
 * @return 0 表示可用；负值表示不可用（上层应退化为内存态）
 */

int storage_init(void);

/**
 * @brief 持久化是否可用
 */

bool storage_available(void);

/**
 * @brief 目录路径（诊断页展示用）
 */

const char *storage_dir(void);

/**
 * @brief 写一个键值（二进制安全）
 * @return 0 成功；负值失败
 */

int storage_save(const char *key, const void *data, size_t len);

/**
 * @brief 读一个键值
 * @return 读到的字节数；键不存在返回 -ENOENT
 */

int storage_load(const char *key, void *data, size_t maxlen);

/**
 * @brief 删除一个键
 */

int storage_remove(const char *key);

#endif /* __APP_SILVER_GUARDIAN_HUB_INCLUDE_STORAGE_H */
