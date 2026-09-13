/****************************************************************************
 * Silver Guardian Hub - 状态指示灯实现
 *
 * 硬件：板载 WS2812B（原理图 LED4），R528 的 LEDC 控制器驱动，
 * NuttX 侧由 nxd/../drivers/leds/ws2812.c 提供 /dev/leds0
 * （板级在 r528_boot.c 里 r528_ws2812_setup("dev/leds0", 1, false) 注册）。
 *
 * 用法就是写一个 uint32 颜色值：open("/dev/leds0") -> write(&color, 4)。
 * 驱动内部 ws2812_write() 会把低 24 位按 LEDC 时序发出去，锁 + 硬件传输，
 * 微秒量级，放在 10ms 主循环里按几百毫秒的节奏写完全没负担。
 *
 * 反面教材：硬件手册的 "SYS_LED 引脚 PB10" 在这块板子上点不亮东西，
 * 真正可见的是这颗 WS2812 —— 别照着手册去翻 GPIO。
 *
 * 所有对外符号带 sg_led_ 前缀，避开厂商 luncher_mini 的 led_* 一族
 * （LTO 全量链接，裸 led_off 会 multiple definition）。
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
#include <syslog.h>

#include <nuttx/clock.h>

#include "include/led.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LOG_TAG      "led"
#define LED_DEVPATH  "/dev/leds0"

/* 各节奏的半周期：亮多久 = 灭多久 */

#define LED_SLOW_HALF_MS  500
#define LED_FAST_HALF_MS  125

/****************************************************************************
 * Private Data
 ****************************************************************************/

static int           g_fd = -1;
static sg_led_diag_t g_diag;

static uint32_t      g_color;          /* 当前示警颜色 */
static uint8_t       g_blink;          /* 当前节奏 */
static uint32_t      g_alert_start;    /* 本次示警开始的时刻 */
static uint32_t      g_hold_ms;        /* 0 = 一直保持 */

/* 灯当前实际颜色缓存。写设备要过一次 LEDC 传输，能省就省：
 * 主循环 10ms 一轮，不缓存的话 1Hz 闪烁也会变成每秒 100 次写。 */

static uint32_t      g_written = 0xFFFFFFFFu;   /* 哨兵：强制首写 */

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static uint32_t led_now_ms(void)
{
  return (uint32_t)TICK2MSEC(clock_systime_ticks());
}

static void led_write_color(uint32_t color)
{
  ssize_t n;

  if (g_fd < 0 || color == g_written)
    {
      return;
    }

  n = write(g_fd, &color, sizeof(color));
  if (n != (ssize_t)sizeof(color))
    {
      syslog(LOG_WARNING, "[%s] 写 %s 失败: 返回 %d, errno=%d\n",
             LOG_TAG, LED_DEVPATH, (int)n, errno);
      return;
    }

  g_written = color;
  g_diag.writes++;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int sg_led_init(void)
{
  memset(&g_diag, 0, sizeof(g_diag));

  g_diag.probed  = true;
  g_diag.devpath = LED_DEVPATH;
  g_diag.color   = SG_LED_COLOR_OFF;
  g_diag.blink   = SG_LED_BLINK_SOLID;

  g_color       = SG_LED_COLOR_OFF;
  g_blink       = SG_LED_BLINK_SOLID;
  g_alert_start = led_now_ms();
  g_hold_ms     = 0;
  g_written     = 0xFFFFFFFFu;

  g_fd = open(LED_DEVPATH, O_RDWR | O_CLOEXEC);
  if (g_fd < 0)
    {
      g_diag.err = errno;
      syslog(LOG_WARNING, "[%s] 打开 %s 失败: %d，无指示灯提示\n",
             LOG_TAG, LED_DEVPATH, g_diag.err);
      return -g_diag.err;
    }

  g_diag.opened = true;

  /* 开机先确保熄灭，免得重启前留下的颜色被误读成告警 */

  led_write_color(SG_LED_COLOR_OFF);

  syslog(LOG_INFO, "[%s] 指示灯就绪: %s (WS2812 RGB)\n",
         LOG_TAG, LED_DEVPATH);

  return OK;
}

void sg_led_deinit(void)
{
  if (g_fd >= 0)
    {
      led_write_color(SG_LED_COLOR_OFF);
      close(g_fd);
      g_fd = -1;
    }

  g_diag.opened = false;
  g_diag.color  = SG_LED_COLOR_OFF;
  g_diag.blink  = SG_LED_BLINK_SOLID;
  g_color       = SG_LED_COLOR_OFF;
  g_blink       = SG_LED_BLINK_SOLID;
  g_written     = 0xFFFFFFFFu;

  syslog(LOG_INFO, "[%s] 指示灯已关闭\n", LOG_TAG);
}

void sg_led_alert(uint32_t color, uint8_t blink, uint32_t hold_ms)
{
  if (color == SG_LED_COLOR_OFF)
    {
      sg_led_off();
      return;
    }

  if (blink > SG_LED_BLINK_FAST)
    {
      syslog(LOG_WARNING, "[%s] 未知闪烁节奏 %u，按常亮处理\n",
             LOG_TAG, (unsigned)blink);
      blink = SG_LED_BLINK_SOLID;
    }

  g_color       = color & 0xFFFFFFu;
  g_blink       = blink;
  g_alert_start = led_now_ms();
  g_hold_ms     = hold_ms;

  g_diag.color = g_color;
  g_diag.blink = blink;

  syslog(LOG_INFO, "[%s] 指示灯 -> #%06lX %s (保持 %lu ms)\n", LOG_TAG,
         (unsigned long)g_color, sg_led_blink_name(blink),
         (unsigned long)hold_ms);
}

void sg_led_off(void)
{
  g_color       = SG_LED_COLOR_OFF;
  g_blink       = SG_LED_BLINK_SOLID;
  g_alert_start = led_now_ms();
  g_hold_ms     = 0;

  g_diag.color = SG_LED_COLOR_OFF;
  g_diag.blink = SG_LED_BLINK_SOLID;

  led_write_color(SG_LED_COLOR_OFF);
}

void sg_led_tick(void)
{
  uint32_t half;
  uint32_t elapsed;

  if (g_fd < 0)
    {
      return;
    }

  if (g_color == SG_LED_COLOR_OFF)
    {
      led_write_color(SG_LED_COLOR_OFF);
      return;
    }

  if (g_blink == SG_LED_BLINK_SOLID)
    {
      led_write_color(g_color);
      return;
    }

  elapsed = led_now_ms() - g_alert_start;

  /* 示警有过期时间：没人理的用药提醒不该一直闪到天亮 */

  if (g_hold_ms != 0 && elapsed >= g_hold_ms)
    {
      sg_led_off();
      return;
    }

  half = (g_blink == SG_LED_BLINK_FAST) ? LED_FAST_HALF_MS : LED_SLOW_HALF_MS;

  led_write_color(((elapsed / half) & 1) ? g_color : SG_LED_COLOR_OFF);
}

bool sg_led_is_available(void)
{
  return g_diag.opened;
}

const sg_led_diag_t *sg_led_get_diag(void)
{
  return &g_diag;
}

const char *sg_led_blink_name(uint8_t blink)
{
  switch (blink)
    {
      case SG_LED_BLINK_SOLID: return "常亮";
      case SG_LED_BLINK_SLOW:  return "慢闪 1Hz";
      case SG_LED_BLINK_FAST:  return "快闪 4Hz";
      default:                 return "未知";
    }
}
