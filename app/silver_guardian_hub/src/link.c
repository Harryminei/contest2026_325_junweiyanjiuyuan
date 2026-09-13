/****************************************************************************
 * Silver Guardian Hub - 板间联动接口实现（Hub 侧接收端）
 *
 * 收包在独立线程，但**事件的产生放在主循环**：event_send() 操作的是全局
 * 事件队列且没有加锁，多线程写会坏。所以线程只做「收 + 入队」，
 * 主循环调 sg_link_poll() 出队并翻译成事件。
 *
 * 队列满时丢最旧的策略不适用（会丢告警），这里是**丢新的并计数** ——
 * 告警场景下宁可让发送端重发，也不能把已排队的 SOS 挤掉。
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <semaphore.h>
#include <pthread.h>

#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>

#include <nuttx/clock.h>

#include "include/link.h"
#include "include/event.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LOG_TAG           "link"
#define LINK_RX_QUEUE     8
#define LINK_THREAD_STACK 8192
#define LINK_RECV_TIMEOUT 1      /* 收包超时（秒），用来周期性检查退出标志 */

/****************************************************************************
 * Private Data
 ****************************************************************************/

static int            g_sock = -1;
static volatile bool  g_thread_run;
static pthread_t      g_thread;

static sem_t          g_lock;
static char           g_rx[LINK_RX_QUEUE][SG_LINK_LINE_MAX];
static int            g_rx_head;
static int            g_rx_tail;
static int            g_rx_count;

static sg_link_diag_t g_diag;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static uint32_t link_now_ms(void)
{
  return (uint32_t)TICK2MSEC(clock_systime_ticks());
}

static void trim_eol(char *s)
{
  size_t n = strlen(s);

  while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' ||
                   s[n - 1] == ' '  || s[n - 1] == '\t'))
    {
      s[--n] = '\0';
    }
}

/**
 * @brief 入队一条报文（线程侧调用）
 * @return 0 入队成功，-ENOSPC 队列满
 */

static int rx_push(const char *line)
{
  int ret = OK;

  sem_wait(&g_lock);

  if (g_rx_count >= LINK_RX_QUEUE)
    {
      ret = -ENOSPC;
    }
  else
    {
      strncpy(g_rx[g_rx_tail], line, SG_LINK_LINE_MAX - 1);
      g_rx[g_rx_tail][SG_LINK_LINE_MAX - 1] = '\0';
      g_rx_tail = (g_rx_tail + 1) % LINK_RX_QUEUE;
      g_rx_count++;
    }

  sem_post(&g_lock);
  return ret;
}

/**
 * @brief 出队一条报文（主循环侧调用）
 * @return true 取到了一条
 */

static bool rx_pop(char *out)
{
  bool got = false;

  sem_wait(&g_lock);

  if (g_rx_count > 0)
    {
      strncpy(out, g_rx[g_rx_head], SG_LINK_LINE_MAX - 1);
      out[SG_LINK_LINE_MAX - 1] = '\0';
      g_rx_head = (g_rx_head + 1) % LINK_RX_QUEUE;
      g_rx_count--;
      got = true;
    }

  sem_post(&g_lock);
  return got;
}

static void *link_rx_thread(void *arg)
{
  char                buf[SG_LINK_LINE_MAX + 4];
  struct sockaddr_in  from;
  socklen_t           fromlen;
  ssize_t             n;

  (void)arg;

  while (g_thread_run)
    {
      fromlen = sizeof(from);
      memset(&from, 0, sizeof(from));

      n = recvfrom(g_sock, buf, SG_LINK_LINE_MAX, 0,
                   (struct sockaddr *)&from, &fromlen);

      if (n <= 0)
        {
          /* 收包设了超时，超时是正常现象，借这机会看一眼退出标志 */

          if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK &&
              errno != EINTR && g_thread_run)
            {
              syslog(LOG_WARNING, "[%s] recvfrom 失败: %d\n", LOG_TAG, errno);
              usleep(200 * 1000);
            }

          continue;
        }

      if (n >= (ssize_t)sizeof(buf))
        {
          n = sizeof(buf) - 1;
        }

      buf[n] = '\0';
      trim_eol(buf);

      if (buf[0] == '\0')
        {
          continue;
        }

      g_diag.last_rx_ms = link_now_ms();

      if (rx_push(buf) != OK)
        {
          g_diag.rx_dropped++;
          syslog(LOG_WARNING, "[%s] 队列满，丢弃: %s\n", LOG_TAG, buf);
        }
    }

  return NULL;
}

/**
 * @brief 把一条报文翻译成事件
 */

static void link_dispatch(const char *line)
{
  const char *p;
  const size_t plen = strlen(SG_LINK_PREFIX);

  if (strncmp(line, SG_LINK_PREFIX, plen) != 0)
    {
      g_diag.rx_dropped++;
      syslog(LOG_WARNING, "[%s] 不是本协议的报文，丢弃: %s\n", LOG_TAG, line);
      return;
    }

  p = line + plen;

  if (strcmp(p, "SOS") == 0)
    {
      syslog(LOG_INFO, "[%s] 手环：紧急求助\n", LOG_TAG);
      event_trigger_sos();
    }
  else if (strcmp(p, "SOS_CANCEL") == 0)
    {
      syslog(LOG_INFO, "[%s] 手环：取消求助\n", LOG_TAG);
      event_trigger_sos_cancel();
    }
  else if (strcmp(p, "FALL") == 0)
    {
      /* 跌倒按紧急求助处理：老人可能已经无法自己按键，
       * 少一级确认比多一级更安全。 */

      syslog(LOG_INFO, "[%s] 手环：跌倒\n", LOG_TAG);
      event_trigger_sos();
    }
  else if (strncmp(p, "SITTING ", 8) == 0)
    {
      uint32_t sec = (uint32_t)strtoul(p + 8, NULL, 10);

      syslog(LOG_INFO, "[%s] 手环：久坐 %lu 秒\n", LOG_TAG,
             (unsigned long)sec);
      event_trigger_sitting(sec);
    }
  else if (strcmp(p, "RESUMED") == 0)
    {
      syslog(LOG_INFO, "[%s] 手环：恢复活动\n", LOG_TAG);
      event_trigger_activity_resumed();
    }
  else if (strcmp(p, "HB") == 0)
    {
      /* 心跳不发事件 —— 它只说明"手环还在"，
       * 每 10 秒往事件记录里塞一条会把记录页刷爆。 */
    }
  else if (strncmp(p, "IMU ", 4) == 0)
    {
      /* 预留：原始加速度。当前只落日志，将来要在 Hub 侧复算姿态时再解析。 */

      syslog(LOG_INFO, "[%s] 手环 IMU: %s\n", LOG_TAG, p + 4);
    }
  else
    {
      g_diag.rx_dropped++;
      syslog(LOG_WARNING, "[%s] 未知报文，丢弃: %s\n", LOG_TAG, line);
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int sg_link_init(void)
{
  struct sockaddr_in  addr;
  struct timeval      tv;
  pthread_attr_t      attr;
  int                 opt = 1;
  int                 ret;

  memset(&g_diag, 0, sizeof(g_diag));
  g_rx_head = g_rx_tail = g_rx_count = 0;

  sem_init(&g_lock, 0, 1);

  g_sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (g_sock < 0)
    {
      g_diag.err = errno;
      syslog(LOG_WARNING, "[%s] 建 socket 失败: %d，无板间联动\n", LOG_TAG,
             g_diag.err);
      return -g_diag.err;
    }

  setsockopt(g_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  tv.tv_sec  = LINK_RECV_TIMEOUT;
  tv.tv_usec = 0;
  setsockopt(g_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  memset(&addr, 0, sizeof(addr));
  addr.sin_family      = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port        = htons(SG_LINK_PORT);

  if (bind(g_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
      g_diag.err = errno;
      syslog(LOG_WARNING, "[%s] 绑 %d 端口失败: %d\n", LOG_TAG,
             SG_LINK_PORT, g_diag.err);
      close(g_sock);
      g_sock = -1;
      return -g_diag.err;
    }

  g_thread_run = true;

  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, LINK_THREAD_STACK);

  ret = pthread_create(&g_thread, &attr, link_rx_thread, NULL);
  pthread_attr_destroy(&attr);

  if (ret != 0)
    {
      g_diag.err = ret;
      syslog(LOG_WARNING, "[%s] 收包线程起不来: %d\n", LOG_TAG, ret);
      close(g_sock);
      g_sock = -1;
      g_thread_run = false;
      return -ret;
    }

  pthread_detach(g_thread);

  g_diag.opened = true;
  syslog(LOG_INFO, "[%s] 板间联动就绪，监听 UDP %d\n", LOG_TAG,
         SG_LINK_PORT);
  return OK;
}

void sg_link_deinit(void)
{
  g_thread_run = false;

  if (g_sock >= 0)
    {
      close(g_sock);
      g_sock = -1;
    }

  sem_destroy(&g_lock);
  g_diag.opened = false;
  g_diag.peer_online = false;
}

void sg_link_poll(void)
{
  char line[SG_LINK_LINE_MAX];
  int  drained = 0;

  if (!g_diag.opened)
    {
      return;
    }

  /* 手环在线判定：靠心跳。没有心跳就当离线，界面上如实显示。 */

  g_diag.peer_online = (g_diag.last_rx_ms != 0) &&
                       ((uint32_t)(link_now_ms() - g_diag.last_rx_ms) <
                        SG_LINK_PEER_TIMEOUT_MS);

  /* 一轮最多处理 4 条，避免报文突发时把主循环卡住 */

  while (drained < 4 && rx_pop(line))
    {
      link_dispatch(line);
      g_diag.rx_lines++;
      drained++;
    }
}

const sg_link_diag_t *sg_link_get_diag(void)
{
  return &g_diag;
}
