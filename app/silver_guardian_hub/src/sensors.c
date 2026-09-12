/****************************************************************************
 * Silver Guardian Hub - 板上传感器实现
 *
 * 走 NuttX uORB 传感器框架：open("/dev/uorb/sensor_xxx0") 之后用 read() 取
 * 最新一次采样。设备默认不开流，所以先 SNIOC_SET_INTERVAL 设一个采样周期，
 * 再以非阻塞方式读——读不到就沿用上一次的值，不阻塞主循环。
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
#include <sys/ioctl.h>
#include <syslog.h>

#include <nuttx/sensors/sensor.h>
#include <nuttx/sensors/ioctl.h>   /* SNIOC_SET_INTERVAL */
#include <nuttx/uorb.h>            /* struct sensor_temp / humi / light ... */

#include "include/sensors.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LOG_TAG          "sensors"
#define SENSOR_REFRESH_MS 1000      /* 刷新周期 */
#define SENSOR_INTERVAL_US 1000000  /* 向驱动请求的采样周期 */

/****************************************************************************
 * Private Data
 ****************************************************************************/

static sensors_state_t g_state;
static uint32_t        g_last_poll_ms;
static bool            g_initialized;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/* 打开单个通道；失败只记录，不返回错误给上层 */

static void open_channel(sg_sensor_t *ch)
{
  int interval = SENSOR_INTERVAL_US;

  ch->fd     = -1;
  ch->opened = false;
  ch->valid  = false;
  ch->value  = 0.0f;
  ch->err    = 0;

  ch->fd = open(ch->path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  if (ch->fd < 0)
    {
      ch->err = errno;
      syslog(LOG_INFO, "[%s] %s (%s) 不可用: %d\n",
             LOG_TAG, ch->name, ch->path, ch->err);
      return;
    }

  /* 设采样周期；驱动不支持也无所谓，退化成"有数据就读" */

  if (ioctl(ch->fd, SNIOC_SET_INTERVAL, (unsigned long)interval) < 0)
    {
      syslog(LOG_INFO, "[%s] %s 设置采样周期失败(忽略): %d\n",
             LOG_TAG, ch->name, errno);
    }

  ch->opened = true;
  syslog(LOG_INFO, "[%s] %s (%s) 就绪\n", LOG_TAG, ch->name, ch->path);
}

/* 读一次；返回 true 表示拿到了新值 */

static bool read_channel(sg_sensor_t *ch, void *buf, size_t buflen)
{
  ssize_t n;

  if (!ch->opened || ch->fd < 0)
    {
      return false;
    }

  memset(buf, 0, buflen);
  n = read(ch->fd, buf, buflen);

  if (n < 0)
    {
      if (errno != EAGAIN && errno != EINTR)
        {
          syslog(LOG_WARNING, "[%s] %s 读取失败: %d\n",
                 LOG_TAG, ch->name, errno);
        }

      return false;
    }

  return (size_t)n == buflen;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int sensors_init(void)
{
  memset(&g_state, 0, sizeof(g_state));

  /* 注意：SHTC3 的温度注册成 SENSOR_TYPE_AMBIENT_TEMP -> "ambient_temp"，
   * 节点是 /dev/uorb/sensor_ambient_temp0，**不是** sensor_temp0。
   * 这个是从板子上 `ls /dev/uorb` 实测出来的，写错的话温度永远打不开。 */

  g_state.ch[SG_SENSOR_TEMP].name  = "温度";
  g_state.ch[SG_SENSOR_TEMP].unit  = "°C";
  g_state.ch[SG_SENSOR_TEMP].path  = "/dev/uorb/sensor_ambient_temp0";

  g_state.ch[SG_SENSOR_HUMI].name  = "湿度";
  g_state.ch[SG_SENSOR_HUMI].unit  = "%";
  g_state.ch[SG_SENSOR_HUMI].path  = "/dev/uorb/sensor_humi0";

  g_state.ch[SG_SENSOR_LIGHT].name = "光照";
  g_state.ch[SG_SENSOR_LIGHT].unit = "lux";
  g_state.ch[SG_SENSOR_LIGHT].path = "/dev/uorb/sensor_light0";

  g_state.ch[SG_SENSOR_PROX].name  = "接近";
  g_state.ch[SG_SENSOR_PROX].unit  = "cm";
  g_state.ch[SG_SENSOR_PROX].path  = "/dev/uorb/sensor_prox0";

  g_state.ch[SG_SENSOR_CO2].name   = "CO2";
  g_state.ch[SG_SENSOR_CO2].unit   = "ppm";
  g_state.ch[SG_SENSOR_CO2].path   = "/dev/uorb/sensor_co20";

  g_state.ch[SG_SENSOR_TVOC].name  = "TVOC";
  g_state.ch[SG_SENSOR_TVOC].unit  = "ppm";
  g_state.ch[SG_SENSOR_TVOC].path  = "/dev/uorb/sensor_tvoc0";

  g_state.opened_count = 0;

  for (int i = 0; i < SG_SENSOR_COUNT; i++)
    {
      open_channel(&g_state.ch[i]);
      if (g_state.ch[i].opened)
        {
          g_state.opened_count++;
        }
    }

  g_initialized = true;

  syslog(LOG_INFO, "[%s] %d/%d 个传感器通道可用\n",
         LOG_TAG, g_state.opened_count, SG_SENSOR_COUNT);

  return g_state.opened_count;
}

void sensors_deinit(void)
{
  for (int i = 0; i < SG_SENSOR_COUNT; i++)
    {
      if (g_state.ch[i].fd >= 0)
        {
          close(g_state.ch[i].fd);
          g_state.ch[i].fd     = -1;
          g_state.ch[i].opened = false;
        }
    }

  g_initialized = false;
}

void sensors_poll(uint32_t now_ms)
{
  struct sensor_temp  temp;
  struct sensor_humi  humi;
  struct sensor_light light;
  struct sensor_prox  prox;
  struct sensor_co2   co2;
  struct sensor_tvoc  tvoc;

  if (!g_initialized)
    {
      return;
    }

  if (g_state.update_count > 0 &&
      (uint32_t)(now_ms - g_last_poll_ms) < SENSOR_REFRESH_MS)
    {
      return;
    }

  g_last_poll_ms = now_ms;
  g_state.update_count++;

  if (read_channel(&g_state.ch[SG_SENSOR_TEMP], &temp, sizeof(temp)))
    {
      g_state.ch[SG_SENSOR_TEMP].value = temp.temperature;
      g_state.ch[SG_SENSOR_TEMP].valid = true;
    }

  if (read_channel(&g_state.ch[SG_SENSOR_HUMI], &humi, sizeof(humi)))
    {
      g_state.ch[SG_SENSOR_HUMI].value = humi.humidity;
      g_state.ch[SG_SENSOR_HUMI].valid = true;
    }

  if (read_channel(&g_state.ch[SG_SENSOR_LIGHT], &light, sizeof(light)))
    {
      g_state.ch[SG_SENSOR_LIGHT].value = light.light;
      g_state.ch[SG_SENSOR_LIGHT].valid = true;
    }

  if (read_channel(&g_state.ch[SG_SENSOR_PROX], &prox, sizeof(prox)))
    {
      g_state.ch[SG_SENSOR_PROX].value = prox.proximity;
      g_state.ch[SG_SENSOR_PROX].valid = true;
    }

  if (read_channel(&g_state.ch[SG_SENSOR_CO2], &co2, sizeof(co2)))
    {
      g_state.ch[SG_SENSOR_CO2].value = co2.co2;
      g_state.ch[SG_SENSOR_CO2].valid = true;
    }

  if (read_channel(&g_state.ch[SG_SENSOR_TVOC], &tvoc, sizeof(tvoc)))
    {
      g_state.ch[SG_SENSOR_TVOC].value = tvoc.tvoc;
      g_state.ch[SG_SENSOR_TVOC].valid = true;
    }

  g_state.last_update_ms = now_ms;
}

const sensors_state_t *sensors_get(void)
{
  return &g_state;
}

bool sensors_is_present(int index)
{
  if (index < 0 || index >= SG_SENSOR_COUNT)
    {
      return false;
    }

  return g_state.ch[index].opened;
}
