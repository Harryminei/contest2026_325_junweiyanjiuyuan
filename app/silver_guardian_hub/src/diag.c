/****************************************************************************
 * Silver Guardian Hub - 自检报告实现
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>
#include <syslog.h>

#include "include/diag.h"
#include "include/storage.h"
#include "include/sensors.h"
#include "include/audio.h"
#include "include/led.h"
#include "include/link.h"
#include "include/touch.h"
#include "include/event.h"
#include "include/medication.h"
#include "include/lcd.h"
#include "include/ui.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LOG_TAG   "diag"
#define DIAG_PATH "/data/silver_guardian/diag.txt"

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const char *g_audio_fail_stage;
static int         g_audio_fail_err;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/**
 * @brief 列一个目录里的条目，逗号分隔写进 buf
 */

static void list_dir(const char *path, char *buf, size_t buflen)
{
  DIR *dir;
  struct dirent *ent;
  size_t used = 0;

  buf[0] = '\0';

  dir = opendir(path);
  if (dir == NULL)
    {
      snprintf(buf, buflen, "(打不开, errno=%d)", errno);
      return;
    }

  while ((ent = readdir(dir)) != NULL)
    {
      if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
        {
          continue;
        }

      int n = snprintf(buf + used, buflen - used, "%s%s",
                       used ? " " : "", ent->d_name);
      if (n < 0 || (size_t)n >= buflen - used)
        {
          break;
        }

      used += (size_t)n;
    }

  closedir(dir);

  if (used == 0)
    {
      snprintf(buf, buflen, "(空)");
    }
}

/**
 * @brief 取当前时间字符串
 */

static void now_str(char *buf, size_t len)
{
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);

  snprintf(buf, len, "%04d-%02d-%02d %02d:%02d:%02d",
           tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
           tm->tm_hour, tm->tm_min, tm->tm_sec);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void diag_note_audio_fail(const char *stage, int err)
{
  g_audio_fail_stage = stage;
  g_audio_fail_err   = err;
}

const char *diag_audio_fail_stage(void)
{
  return g_audio_fail_stage;
}

int diag_audio_fail_err(void)
{
  return g_audio_fail_err;
}

int diag_dump(const char *reason)
{
  const sensors_state_t *st = sensors_get();
  const audio_status_t  *as = audio_get_status();
  const touch_diag_t    *td = touch_probe_get();
  const sg_led_diag_t      *ld = sg_led_get_diag();
  const sg_link_diag_t     *lk = sg_link_get_diag();
  FILE *fp;
  char   buf[512];
  char   ts[32];
  int    i;

  if (!storage_available())
    {
      return -ENODEV;
    }

  fp = fopen(DIAG_PATH, "w");
  if (fp == NULL)
    {
      syslog(LOG_WARNING, "[%s] 打不开 %s: %d\n", LOG_TAG, DIAG_PATH, errno);
      return -errno;
    }

  now_str(ts, sizeof(ts));

  fprintf(fp, "银发守护 Hub 自检报告\n");
  fprintf(fp, "生成时间: %s\n", ts);
  fprintf(fp, "原因    : %s\n", reason ? reason : "-");
  fprintf(fp, "版本    : %s\n", SG_HUB_VERSION);

  /* 构建标记：纯 ASCII，便于用 adb 一眼确认板上跑的是哪一版
   * （踩过：烧错镜像，排查半天才发现板上是旧固件） */

  fprintf(fp, "构建标记: %s\n", SG_HUB_BUILD_TAG);

  /*--------------------------------------------------------------- 触摸 */

  fprintf(fp, "\n[触摸] LVGL indev: %s\n", lcd_touch_ready() ? "ok" : "无");
  fprintf(fp, "  /dev/input0 : %s", td->opened ? "打开成功" : "打开失败");
  if (!td->opened)
    {
      fprintf(fp, " (errno=%d)", td->err);
    }
  fprintf(fp, "\n");
  fprintf(fp, "  maxpoint    : %u\n", td->maxpoint);
  fprintf(fp, "  样本/按下/移动/抬起/空: %lu / %lu / %lu / %lu / %lu\n",
          (unsigned long)td->samples, (unsigned long)td->downs,
          (unsigned long)td->moves, (unsigned long)td->ups,
          (unsigned long)td->empty);
  fprintf(fp, "  最后坐标    : x=%d y=%d flags=0x%02x npoints=%d\n",
          (int)td->last_x, (int)td->last_y, td->last_flags,
          (int)td->last_npoints);
  fprintf(fp, "  观测坐标范围: x[%d,%d] y[%d,%d]  (屏幕 320x240)\n",
          (int)td->min_x, (int)td->max_x, (int)td->min_y, (int)td->max_y);

  /*--------------------------------------------------------------- 音频 */

  fprintf(fp, "\n[音频] 状态: %s\n", as->opened ? "就绪" : "不可用");
  fprintf(fp, "  设备节点    : %s\n", as->devpath ? as->devpath : "(未打开)");
  if (!as->opened)
    {
      fprintf(fp, "  打开失败 errno: %d\n", as->err);
    }
  fprintf(fp, "  累计播放/丢弃: %lu / %lu\n",
          (unsigned long)as->played, (unsigned long)as->dropped);
  fprintf(fp, "  音量        : %u\n", as->volume);
  fprintf(fp, "  驱动缓冲    : 单块 %lu 字节, 实拿 %lu 块\n",
          (unsigned long)as->buf_size, (unsigned long)as->buf_count);
  fprintf(fp, "  上次送数据  : %lu / %lu 字节%s\n",
          (unsigned long)as->last_bytes, (unsigned long)as->last_total,
          (as->last_total > 0 && as->last_bytes < as->last_total)
              ? "  <- 被截断，提示音只播了一部分" : "");

  if (g_audio_fail_stage != NULL)
    {
      fprintf(fp, "  !! 最近一次播放失败于 %s, errno=%d\n",
              g_audio_fail_stage, g_audio_fail_err);
    }
  else if (as->played > 0)
    {
      fprintf(fp, "  最近一次播放: 成功（若仍听不到声音，问题在 codec/功放层）\n");
    }

  list_dir("/dev/audio", buf, sizeof(buf));
  fprintf(fp, "  /dev/audio 下: %s\n", buf);

  /*----------------------------------------------------------- 指示灯 */

  fprintf(fp, "\n[指示灯] 状态: %s\n", ld->opened ? "就绪" : "不可用");
  fprintf(fp, "  设备节点    : %s\n", ld->devpath ? ld->devpath : "(未打开)");
  if (!ld->opened)
    {
      fprintf(fp, "  打开失败 errno: %d\n", ld->err);
    }
  fprintf(fp, "  当前颜色    : #%06lX %s\n", (unsigned long)ld->color,
          sg_led_blink_name(ld->blink));
  fprintf(fp, "  累计写设备  : %lu 次\n", (unsigned long)ld->writes);

  /*--------------------------------------------------------- 板间联动 */

  fprintf(fp, "\n[联动] 监听: %s (UDP %d)\n",
          lk->opened ? "就绪" : "不可用", SG_LINK_PORT);
  if (!lk->opened)
    {
      fprintf(fp, "  建立失败 errno: %d\n", lk->err);
    }
  fprintf(fp, "  手环在线    : %s\n", lk->peer_online ? "是" : "否");
  fprintf(fp, "  累计收报文  : %lu 条, 丢弃 %lu 条\n",
          (unsigned long)lk->rx_lines, (unsigned long)lk->rx_dropped);
  fprintf(fp, "  最近收到    : %s\n",
          (lk->last_rx_ms != 0) ? "有" : "还没有过");

  /*------------------------------------------------------------- 传感器 */

  fprintf(fp, "\n[传感器] 可用 %d/%d\n",
          st->opened_count, SG_SENSOR_COUNT);

  for (i = 0; i < SG_SENSOR_COUNT; i++)
    {
      const sg_sensor_t *ch = &st->ch[i];

      fprintf(fp, "  %-4s %-32s %s", ch->name, ch->path,
              ch->opened ? "打开成功" : "打不开");

      if (!ch->opened)
        {
          fprintf(fp, " (errno=%d)", ch->err);
        }
      else if (ch->valid)
        {
          fprintf(fp, " 值=%.2f %s", (double)ch->value, ch->unit);
        }
      else
        {
          fprintf(fp, " 但一直没读到值");
        }

      fprintf(fp, "\n");
    }

  list_dir("/dev/uorb", buf, sizeof(buf));
  fprintf(fp, "  /dev/uorb 下: %s\n", buf);

  /*------------------------------------------------------- 存储 / 事件 */

  fprintf(fp, "\n[存储] %s, 目录 %s\n",
          storage_available() ? "可写" : "不可用", storage_dir());

  fprintf(fp, "\n[用药] 计划 %d 条\n", medication_get_plan_count());

  for (i = 0; i < medication_get_plan_count() && i < 10; i++)
    {
      const medication_plan_t *p = medication_get_plan(i);

      if (p != NULL)
        {
          fprintf(fp, "  %02d:%02d %s %d%s  %s%s\n",
                  p->hour, p->minute, p->name, (int)p->dosage, p->unit,
                  p->enabled ? "启用" : "停用",
                  p->taken ? " 今日已服" : "");
        }
    }

  fprintf(fp, "\n[事件] 队列 %d 条\n", event_get_count());
  fprintf(fp, "\n[界面] 当前页 %d, 演示模式 %s\n",
          (int)ui_current_page(), lcd_get_demo_mode() ? "开" : "关");

  fclose(fp);

  syslog(LOG_INFO, "[%s] 自检报告已写入 %s\n", LOG_TAG, DIAG_PATH);
  return OK;
}
