/****************************************************************************
 * Silver Guardian Hub - SNTP 时间同步实现
 *
 * 一个最小的 SNTP 客户端：发一个 48 字节的 NTP v3 请求，从回包里取
 * "Transmit Timestamp"（第 40~43 字节，大端 32 位秒数），换算成 Unix 时间。
 *
 * NTP 的纪元是 1900-01-01，Unix 是 1970-01-01，差 2208988800 秒。
 * NTP 2036 年翻转问题这里不管 —— 那之前这颗芯片早退役了。
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <netinet/in.h>

#include "include/sntp.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LOG_TAG       "sntp"
#define SNTP_PKT_LEN  48
#define SNTP_PORT     "123"
#define NTP_TO_UNIX   2208988800ul

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* 国内服务器排前面：现场演示时快几十毫秒也是好的。
 * 后面两个是全球池，换个网络环境也能兜住。 */

static const char *const g_sntp_servers[] =
{
  "ntp.aliyun.com",
  "cn.pool.ntp.org",
  "pool.ntp.org",
  NULL
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/**
 * @brief 向一台服务器要一次时间
 * @param host 主机名
 * @param out  成功时写入 Unix 秒
 */

static int sntp_query(const char *host, time_t *out)
{
  struct addrinfo  hints;
  struct addrinfo *res = NULL;
  struct timeval   tv;
  uint8_t          pkt[SNTP_PKT_LEN];
  uint32_t         secs;
  ssize_t          n;
  int              sock;
  int              ret;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family   = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;

  ret = getaddrinfo(host, SNTP_PORT, &hints, &res);
  if (ret != 0 || res == NULL)
    {
      syslog(LOG_WARNING, "[%s] 解析不了 %s\n", LOG_TAG, host);
      return -EHOSTUNREACH;
    }

  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0)
    {
      freeaddrinfo(res);
      return -errno;
    }

  tv.tv_sec  = SG_SNTP_TIMEOUT_SEC;
  tv.tv_usec = 0;
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  /* LI=0 无告警, VN=3, Mode=3(客户端) —— 即 0x1B，其余字段留 0 即可 */

  memset(pkt, 0, sizeof(pkt));
  pkt[0] = 0x1B;

  n = sendto(sock, pkt, sizeof(pkt), 0, res->ai_addr, res->ai_addrlen);
  if (n != (ssize_t)sizeof(pkt))
    {
      syslog(LOG_WARNING, "[%s] 发往 %s 失败: %d\n", LOG_TAG, host, errno);
      close(sock);
      freeaddrinfo(res);
      return -errno;
    }

  n = recv(sock, pkt, sizeof(pkt), 0);

  close(sock);
  freeaddrinfo(res);

  if (n < (ssize_t)SNTP_PKT_LEN)
    {
      syslog(LOG_WARNING, "[%s] %s 没回包(%d)\n", LOG_TAG, host, (int)n);
      return -ETIMEDOUT;
    }

  /* Transmit Timestamp：大端 32 位秒（小数部分在 44~47，用不上） */

  secs = ((uint32_t)pkt[40] << 24) | ((uint32_t)pkt[41] << 16) |
         ((uint32_t)pkt[42] << 8)  |  (uint32_t)pkt[43];

  if (secs < NTP_TO_UNIX)
    {
      syslog(LOG_WARNING, "[%s] %s 回包时间不合理\n", LOG_TAG, host);
      return -EINVAL;
    }

  *out = (time_t)(secs - NTP_TO_UNIX);
  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int sg_sntp_sync(void)
{
  struct timeval tv;
  time_t         when;
  int            i;

  for (i = 0; g_sntp_servers[i] != NULL; i++)
    {
      if (sntp_query(g_sntp_servers[i], &when) != OK)
        {
          continue;
        }

      tv.tv_sec  = when;
      tv.tv_usec = 0;

      if (settimeofday(&tv, NULL) != 0)
        {
          syslog(LOG_ERR, "[%s] settimeofday 失败: %d\n", LOG_TAG, errno);
          return -errno;
        }

      syslog(LOG_INFO, "[%s] 校时成功，来自 %s\n", LOG_TAG,
             g_sntp_servers[i]);
      return OK;
    }

  syslog(LOG_WARNING, "[%s] 所有时间服务器都没回应\n", LOG_TAG);
  return -ETIMEDOUT;
}
