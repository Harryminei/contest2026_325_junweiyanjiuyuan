/****************************************************************************
 * Silver Guardian Hub - 触摸诊断模块实现
 *
 * 直接读 /dev/input0（NuttX touchscreen 上层），把原始样本记录成统计量，
 * 供"触摸自检"页面显示。这条路径与 LVGL 的 indev 完全独立：
 * touch_event() 会把样本广播给所有已打开的 fd，所以两个读者互不影响。
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
#include <sys/ioctl.h>
#include <syslog.h>

#include <nuttx/clock.h>
#include <nuttx/input/touchscreen.h>

#include "include/touch.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LOG_TAG       "touch"
#define TOUCH_DEVPATH "/dev/input0"

/****************************************************************************
 * Private Data
 ****************************************************************************/

static int          g_fd = -1;
static touch_diag_t g_diag;
static uint8_t     *g_buf;        /* 按 maxpoint 定长的样本缓冲 */
static size_t       g_bufsize;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static uint32_t now_ms(void)
{
  return (uint32_t)TICK2MSEC(clock_systime_ticks());
}

static void track_range(int16_t x, int16_t y)
{
  if (g_diag.samples == 1)
    {
      g_diag.min_x = g_diag.max_x = x;
      g_diag.min_y = g_diag.max_y = y;
      return;
    }

  if (x < g_diag.min_x) g_diag.min_x = x;
  if (x > g_diag.max_x) g_diag.max_x = x;
  if (y < g_diag.min_y) g_diag.min_y = y;
  if (y > g_diag.max_y) g_diag.max_y = y;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int touch_probe_init(void)
{
  uint8_t maxpoint = 0;

  memset(&g_diag, 0, sizeof(g_diag));
  g_diag.probed = true;
  g_diag.err    = 0;

  g_fd = open(TOUCH_DEVPATH, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  if (g_fd < 0)
    {
      g_diag.err = errno;
      syslog(LOG_ERR, "[%s] 打开 %s 失败: %d\n", LOG_TAG, TOUCH_DEVPATH,
             g_diag.err);
      return -g_diag.err;
    }

  if (ioctl(g_fd, TSIOC_GETMAXPOINTS, &maxpoint) < 0)
    {
      g_diag.err = errno;
      syslog(LOG_ERR, "[%s] TSIOC_GETMAXPOINTS 失败: %d\n", LOG_TAG,
             g_diag.err);
      close(g_fd);
      g_fd = -1;
      return -g_diag.err;
    }

  if (maxpoint == 0)
    {
      syslog(LOG_ERR, "[%s] maxpoint 为 0，触摸驱动未就绪\n", LOG_TAG);
      close(g_fd);
      g_fd = -1;
      return -ENODEV;
    }

  g_diag.maxpoint = maxpoint;
  g_bufsize = SIZEOF_TOUCH_SAMPLE_S(maxpoint);
  g_buf = malloc(g_bufsize);
  if (g_buf == NULL)
    {
      syslog(LOG_ERR, "[%s] 样本缓冲分配失败 (%u 字节)\n", LOG_TAG,
             (unsigned)g_bufsize);
      close(g_fd);
      g_fd = -1;
      return -ENOMEM;
    }

  memset(g_buf, 0, g_bufsize);
  g_diag.opened = true;

  syslog(LOG_INFO, "[%s] %s 打开成功, maxpoint=%u, 样本长度=%u\n",
         LOG_TAG, TOUCH_DEVPATH, maxpoint, (unsigned)g_bufsize);

  return OK;
}

void touch_probe_deinit(void)
{
  if (g_fd >= 0)
    {
      close(g_fd);
      g_fd = -1;
    }

  if (g_buf != NULL)
    {
      free(g_buf);
      g_buf = NULL;
    }

  g_diag.opened = false;
}

void touch_probe_poll(void)
{
  struct touch_sample_s *sample;
  ssize_t n;
  int i;
  bool any = false;

  if (g_fd < 0 || g_buf == NULL)
    {
      return;
    }

  g_diag.polls++;

  /* 一次 poll 最多取若干个样本，避免触摸长按时把主循环堵住 */

  for (i = 0; i < 8; i++)
    {
      memset(g_buf, 0, g_bufsize);
      n = read(g_fd, g_buf, g_bufsize);

      if (n < 0)
        {
          if (errno != EAGAIN && errno != EINTR)
            {
              syslog(LOG_ERR, "[%s] read 失败: %d\n", LOG_TAG, errno);
            }

          break;
        }

      if (n == 0 || (size_t)n < g_bufsize)
        {
          /* 数据长度对不上，说明设备端样本尺寸与 maxpoint 不一致 */

          g_diag.empty++;
          syslog(LOG_WARNING, "[%s] 样本长度异常: 读到 %d, 期望 %u\n",
                 LOG_TAG, (int)n, (unsigned)g_bufsize);
          break;
        }

      sample = (struct touch_sample_s *)g_buf;

      if (sample->npoints <= 0)
        {
          g_diag.empty++;
          continue;
        }

      g_diag.samples++;
      g_diag.last_npoints = sample->npoints;
      g_diag.last_flags   = sample->point[0].flags;
      g_diag.last_x       = sample->point[0].x;
      g_diag.last_y       = sample->point[0].y;
      g_diag.last_ms      = now_ms();

      if (sample->point[0].flags & TOUCH_DOWN) g_diag.downs++;
      if (sample->point[0].flags & TOUCH_MOVE) g_diag.moves++;
      if (sample->point[0].flags & TOUCH_UP)   g_diag.ups++;

      if (sample->point[0].flags & (TOUCH_DOWN | TOUCH_MOVE))
        {
          track_range(sample->point[0].x, sample->point[0].y);
        }

      any = true;
    }

  if (any && g_diag.samples <= 20)
    {
      /* 前 20 个样本逐条打印，方便串口直接看清事件内容 */

      syslog(LOG_INFO, "[%s] #%u n=%d flags=0x%02x x=%d y=%d\n",
             LOG_TAG, g_diag.samples, (int)g_diag.last_npoints,
             g_diag.last_flags, (int)g_diag.last_x, (int)g_diag.last_y);
    }
}

const touch_diag_t *touch_probe_get(void)
{
  return &g_diag;
}

bool touch_probe_has_event(void)
{
  return g_diag.samples > 0;
}

void touch_probe_reset_stats(void)
{
  uint8_t maxpoint = g_diag.maxpoint;
  bool    opened   = g_diag.opened;
  bool    probed   = g_diag.probed;

  memset(&g_diag, 0, sizeof(g_diag));
  g_diag.probed   = probed;
  g_diag.opened   = opened;
  g_diag.maxpoint = maxpoint;
}
