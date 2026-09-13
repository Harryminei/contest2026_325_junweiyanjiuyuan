/****************************************************************************
 * Silver Guardian Hub - WiFi 操作实现
 *
 * 用 posix_spawn 调板上的 wapi 命令。命令行的每个参数、以及下面那套顺序，
 * 都是 2026-09-13 在真机上一条条试出来的（见 docs 里的联调记录）。
 *
 * 关键顺序（改了就连不上，别动）：
 *   1. freq 必须最先、且必须 FIXED
 *      —— 驱动关联时只在"已配置的频段"里找 AP。出厂配置是 2.4G，
 *         而目标 AP 在 5GHz 时，日志会一直报
 *         `rtw_select_and_join_from_scanned_queue: _FAIL(candidate == NULL)`。
 *         先把频率钉到 AP 所在信道，关联才能成功。
 *   2. DHCP 之前必须先把 IP 清成 0.0.0.0
 *      —— 板子开机自带静态 10.0.0.2/网关 10.0.0.1，带着它跑 renew 会
 *         `netlib_obtain_ipv4addr() failed`。
 *   3. save_config 必须在【已拿到 IP】之后跑
 *      —— 它保存的是驱动"当前状态"，没连上时跑等于把旧配置又存一遍。
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <spawn.h>
#include <signal.h>
#include <sys/wait.h>
#include <syslog.h>

#include "include/wifi.h"
#include "include/net.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LOG_TAG       "wifi"
#define WAPI_PROG     "wapi"
#define RENEW_PROG    "renew"

/* 抓命令输出的临时文件（/data/silver_guardian 由 storage_init 建好） */

#define WAPI_OUT_PATH "/data/silver_guardian/.wapi.out"
#define WAPI_OUT_MAX  8192

/* 等子进程的轮询粒度 */

#define WAIT_SLICE_MS 50

/****************************************************************************
 * Private Data
 ****************************************************************************/

static char * const g_empty_env[] = { NULL };

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/**
 * @brief 跑一个 builtin，可选把 stdout+stderr 抓下来
 *
 * 子进程卡住时不能把主循环堵死，所以用 WNOHANG 轮询 + 超时强杀。
 *
 * @param prog   程序名（"wapi" / "renew"）
 * @param out    输出缓冲，NULL 表示不抓输出
 * @param args   以 NULL 结尾的参数数组（不含程序名）
 * @return 0 成功；-ETIMEDOUT 超时；其他负值为 spawn 错误
 */

static int run_capture(const char *prog, char *out, size_t outlen,
                       int timeout_ms, const char *const *args)
{
  posix_spawn_file_actions_t fa;
  const char  *argv[10];
  int          argc = 0;
  int          ret;
  int          waited = 0;
  int          status = 0;
  pid_t        pid;
  int          i;

  if (out != NULL && outlen > 0)
    {
      out[0] = '\0';
    }

  argv[argc++] = prog;
  for (i = 0; args != NULL && args[i] != NULL && argc < 9; i++)
    {
      argv[argc++] = args[i];
    }

  argv[argc] = NULL;

  posix_spawn_file_actions_init(&fa);

  if (out != NULL)
    {
      /* stdout 截断建文件，再把 stderr 并过去 —— wapi 的错误信息走 stderr，
       * 不并的话出错时什么都看不到。 */

      posix_spawn_file_actions_addopen(&fa, 1, WAPI_OUT_PATH,
                                       O_WRONLY | O_CREAT | O_TRUNC, 0666);
      posix_spawn_file_actions_adddup2(&fa, 1, 2);
    }

  ret = posix_spawn(&pid, prog, &fa, NULL, (char * const *)argv,
                    g_empty_env);
  posix_spawn_file_actions_destroy(&fa);

  if (ret != 0)
    {
      syslog(LOG_ERR, "[%s] 起不了 %s: %d\n", LOG_TAG, prog, ret);
      return -ret;
    }

  while (waited < timeout_ms)
    {
      ret = waitpid(pid, &status, WNOHANG);

      if (ret == pid)
        {
          break;
        }

      if (ret < 0 && errno != EINTR)
        {
          syslog(LOG_ERR, "[%s] waitpid 失败: %d\n", LOG_TAG, errno);
          return -errno;
        }

      usleep(WAIT_SLICE_MS * 1000);
      waited += WAIT_SLICE_MS;
    }

  if (waited >= timeout_ms)
    {
      syslog(LOG_WARNING, "[%s] %s 超时(%d ms)，强杀\n", LOG_TAG, prog,
             timeout_ms);
      kill(pid, SIGKILL);
      waitpid(pid, &status, 0);
      return -ETIMEDOUT;
    }

  if (out != NULL)
    {
      int fd = open(WAPI_OUT_PATH, O_RDONLY);
      if (fd >= 0)
        {
          ssize_t n = read(fd, out, outlen - 1);
          out[(n > 0) ? n : 0] = '\0';
          close(fd);
        }
    }

  return 0;
}

/**
 * @brief 解析 wapi scan_results 的输出
 *
 * 格式（制表符分隔，SSID 可能含空格、也可能为空）：
 *     bssid / frequency / signal level / encode / ssid
 *     14:09:b4:8c:0b:48\t5240\t-60\t0802\tChinaNet-VgsM-5G
 */

static int parse_scan(const char *text, sg_wifi_ap_t *out, int max)
{
  const char *p = text;
  int         count = 0;

  while (*p != '\0' && count < max)
    {
      char  line[192];
      char *fields[5] = { NULL, NULL, NULL, NULL, NULL };
      int   nf = 0;
      char *s;
      const char *eol;
      size_t len;

      eol = strchr(p, '\n');
      len = (eol != NULL) ? (size_t)(eol - p) : strlen(p);
      if (len >= sizeof(line))
        {
          len = sizeof(line) - 1;
        }

      memcpy(line, p, len);
      line[len] = '\0';
      p = (eol != NULL) ? eol + 1 : p + len;

      /* 跳过表头和非数据行 */

      if (strncmp(line, "bssid", 5) == 0 || line[0] == '\0')
        {
          continue;
        }

      s = line;
      while (nf < 5 && s != NULL && *s != '\0')
        {
          fields[nf++] = s;
          s = strchr(s, '\t');
          if (s != NULL)
            {
              *s++ = '\0';
            }
        }

      if (nf < 5)
        {
          continue;
        }

      /* 没有 SSID 的是隐藏网络，界面上没法选，跳过 */

      if (fields[4][0] == '\0')
        {
          continue;
        }

      memset(&out[count], 0, sizeof(out[count]));
      strncpy(out[count].ssid, fields[4], SG_WIFI_SSID_MAX - 1);
      out[count].freq   = (uint32_t)strtoul(fields[1], NULL, 10);
      out[count].rssi   = (int)strtol(fields[2], NULL, 10);
      out[count].encode = (int)strtol(fields[3], NULL, 16);
      count++;
    }

  return count;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int sg_wifi_scan(sg_wifi_ap_t *out, int max)
{
  static char buf[WAPI_OUT_MAX];
  const char *scan_args[]  = { "scan", SG_NET_IFNAME, NULL };
  const char *res_args[]   = { "scan_results", SG_NET_IFNAME, NULL };
  int ret;

  if (out == NULL || max <= 0)
    {
      return -EINVAL;
    }

  /* 先触发一次扫描，再读结果。scan 的输出用不上，但要等它跑完。 */

  ret = run_capture(WAPI_PROG, buf, sizeof(buf), SG_WIFI_SCAN_TIMEOUT_MS,
                    scan_args);
  if (ret < 0)
    {
      return ret;
    }

  ret = run_capture(WAPI_PROG, buf, sizeof(buf), SG_WIFI_SCAN_TIMEOUT_MS,
                    res_args);
  if (ret < 0)
    {
      return ret;
    }

  return parse_scan(buf, out, max);
}

int sg_wifi_connect(const char *ssid, const char *psk, uint32_t freq)
{
  static char buf[WAPI_OUT_MAX];
  char freqstr[16];
  const char *freq_args[]  = { "freq", SG_NET_IFNAME, NULL, "WAPI_FREQ_FIXED",
                               NULL };
  const char *mode_args[]  = { "mode", SG_NET_IFNAME, "WAPI_MODE_MANAGED",
                               NULL };
  const char *essid_args[] = { "essid", SG_NET_IFNAME, NULL, "WAPI_ESSID_ON",
                               NULL };
  const char *psk_args[]   = { "psk", SG_NET_IFNAME, NULL, "WPA_ALG_CCMP",
                               "WPA_VER_2", NULL };
  const char *ip_args[]    = { "ip", SG_NET_IFNAME, "0.0.0.0", NULL };
  const char *renew_args[] = { SG_NET_IFNAME, NULL };
  const char *save_args[]  = { "save_config", SG_NET_IFNAME, NULL };
  int ret;
  int i;

  if (ssid == NULL || ssid[0] == '\0')
    {
      return -EINVAL;
    }

  /* 1. 钉信道 —— 必须在 essid 之前，否则 5GHz 的 AP 永远匹配不到 */

  if (freq > 0)
    {
      snprintf(freqstr, sizeof(freqstr), "%lu", (unsigned long)freq);
      freq_args[2] = freqstr;
      run_capture(WAPI_PROG, buf, sizeof(buf), SG_WIFI_CONNECT_TIMEOUT_MS,
                  freq_args);
    }

  /* 2. 站模式 → 3. SSID → 4. 密码 */

  run_capture(WAPI_PROG, buf, sizeof(buf), SG_WIFI_CONNECT_TIMEOUT_MS,
              mode_args);

  essid_args[2] = ssid;
  ret = run_capture(WAPI_PROG, buf, sizeof(buf), SG_WIFI_CONNECT_TIMEOUT_MS,
                    essid_args);
  if (ret < 0)
    {
      return ret;
    }

  if (psk != NULL && psk[0] != '\0')
    {
      psk_args[2] = psk;
      run_capture(WAPI_PROG, buf, sizeof(buf), SG_WIFI_CONNECT_TIMEOUT_MS,
                  psk_args);
    }

  /* 5. 清掉出厂静态 IP，再 6. DHCP。
   * 关联成功后第一轮 renew 常常还没就绪，给几次机会。 */

  run_capture(WAPI_PROG, buf, sizeof(buf), SG_WIFI_CONNECT_TIMEOUT_MS,
              ip_args);

  for (i = 0; i < 3; i++)
    {
      run_capture(RENEW_PROG, buf, sizeof(buf), SG_WIFI_CONNECT_TIMEOUT_MS,
                  renew_args);

      if (sg_net_wifi_connected())
        {
          break;
        }

      sleep(1);
    }

  if (!sg_net_wifi_connected())
    {
      syslog(LOG_WARNING, "[%s] 关联了但没拿到 IP\n", LOG_TAG);
      return -ENETUNREACH;
    }

  /* 7. 存配置（必须在拿到 IP 之后，存的是驱动当前状态） */

  run_capture(WAPI_PROG, buf, sizeof(buf), SG_WIFI_CONNECT_TIMEOUT_MS,
              save_args);

  syslog(LOG_INFO, "[%s] 已连接 %s\n", LOG_TAG, ssid);
  return OK;
}

int sg_wifi_disconnect(void)
{
  static char buf[WAPI_OUT_MAX];
  const char *args[] = { "disconnect", SG_NET_IFNAME, NULL };

  return run_capture(WAPI_PROG, buf, sizeof(buf), SG_WIFI_CONNECT_TIMEOUT_MS,
                     args);
}
