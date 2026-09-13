/****************************************************************************
 * Silver Guardian Hub - SNTP 时间同步头文件
 *
 * 为什么不直接用板上的 ntpc：
 *   apps/netutils/ntpclient/ntpclient.c 有个 argv 错位 bug ——
 *   ntpc_start_with_list() 把服务器列表放在 argv[0]，
 *   而 ntpc_daemon() 从 argv[1] 取（且没有 argc 校验），
 *   结果 srvs->ntp_servers 恒为 NULL，`ntpcstatus` 永远是 0 样本。
 *
 *   那份代码在 apps/ 树里，repo sync 会重置；而时间这事对"用药提醒"是硬需求，
 *   放在应用自己手里更踏实，也能做到"WiFi 一连上就校时"。
 ****************************************************************************/

#ifndef __APP_SILVER_GUARDIAN_HUB_INCLUDE_SNTP_H
#define __APP_SILVER_GUARDIAN_HUB_INCLUDE_SNTP_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* 单个服务器的等待时间：局域网/宽带下几十毫秒就有回包，3 秒足够 */

#define SG_SNTP_TIMEOUT_SEC  3

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * @brief 从网络校时并写进系统时钟
 *
 * 按内置服务器列表逐个尝试，第一个成功的就用它。**会阻塞**，
 * 最坏情况 = 服务器个数 × SG_SNTP_TIMEOUT_SEC，所以别在主循环里直接调。
 *
 * @return 0 成功；负值为错误码
 */

int sg_sntp_sync(void);

#endif /* __APP_SILVER_GUARDIAN_HUB_INCLUDE_SNTP_H */
