/****************************************************************************
 * Silver Guardian Hub - 网络状态实现
 *
 * 只做一件事：如实回答"wlan0 到底能不能用"。
 *
 * 之前状态栏是假的，因为 cloud.c 是本地桩：
 *     g_cloud_status.wifi_connected = true;   // 硬编码
 * 于是没网也显示"已联网"。这里换成问网络栈本身。
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>

#include <netinet/in.h>
#include <net/if.h>

#include <netutils/netlib.h>

#include "include/net.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LOG_TAG "net"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

bool sg_net_wifi_connected(void)
{
  struct in_addr addr;
  uint8_t        flags = 0;

  memset(&addr, 0, sizeof(addr));

  /* 没有 IP 就是没通。netlib 拿不到地址时返回负值，拿到 0.0.0.0 也算没通。 */

  if (netlib_get_ipv4addr(SG_NET_IFNAME, &addr) < 0)
    {
      return false;
    }

  if (addr.s_addr == 0)
    {
      return false;
    }

  /* 有 IP 但链路掉了的情况（比如 AP 关了）也要算不通 */

  if (netlib_getifstatus(SG_NET_IFNAME, &flags) == 0 &&
      (flags & IFF_RUNNING) == 0)
    {
      return false;
    }

  return true;
}
