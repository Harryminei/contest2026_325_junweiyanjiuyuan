/****************************************************************************
 * Silver Guardian Hub - WiFi 操作头文件
 *
 * 为什么是"调 wapi 命令"而不是调库：板上 wapi 编译成的是**独立程序**
 * （Makefile 里是 PROGNAME，没有 libwapi），应用侧没法直接链它的函数。
 * 好在这套命令行的行为已经在 2026-09-13 联调时逐条验证过，照着调最稳。
 ****************************************************************************/

#ifndef __APP_SILVER_GUARDIAN_HUB_INCLUDE_WIFI_H
#define __APP_SILVER_GUARDIAN_HUB_INCLUDE_WIFI_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdbool.h>
#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define SG_WIFI_SSID_MAX   33
#define SG_WIFI_PSK_MAX    64
#define SG_WIFI_MAX_APS    20

/* 扫描/连接的最长等待（毫秒）。扫描实测 3~5 秒，连接再多留些余量。 */

#define SG_WIFI_SCAN_TIMEOUT_MS     15000
#define SG_WIFI_CONNECT_TIMEOUT_MS  30000

/****************************************************************************
 * Public Types
 ****************************************************************************/

typedef struct
{
  char     ssid[SG_WIFI_SSID_MAX];   /* 空串表示隐藏 SSID，界面里过滤掉 */
  uint32_t freq;                     /* MHz —— 连接时要拿它去钉信道 */
  int      rssi;                     /* dBm，越接近 0 越强 */
  int      encode;                   /* 驱动上报的加密能力位 */
} sg_wifi_ap_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * @brief 扫描附近的 AP
 * @param out 输出数组
 * @param max 数组容量
 * @return 实际写入条数，负值为错误码
 */

int sg_wifi_scan(sg_wifi_ap_t *out, int max);

/**
 * @brief 连接一个 AP
 *
 * 内部按实测跑通的顺序执行：
 *   freq(FIXED) -> mode -> essid -> psk -> 清 IP -> DHCP -> save_config
 *
 * 顺序不能改，理由见实现里的注释（尤其是 freq 必须在 essid 之前）。
 *
 * @param ssid 目标 SSID
 * @param psk  密码；空串或 NULL 表示开放网络
 * @param freq 目标频率（MHz），来自扫描结果
 * @return 0 成功（已拿到 IP），负值为错误码
 */

int sg_wifi_connect(const char *ssid, const char *psk, uint32_t freq);

/**
 * @brief 断开当前连接
 */

int sg_wifi_disconnect(void);

#endif /* __APP_SILVER_GUARDIAN_HUB_INCLUDE_WIFI_H */
