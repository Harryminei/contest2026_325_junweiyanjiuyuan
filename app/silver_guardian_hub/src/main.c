/****************************************************************************
 * Silver Guardian Hub - Hub端主程序
 *
 * 银发守护系统 - Hub端核心功能
 * 硬件平台：润芯微 Gemini-S1（R528-S3 + 2.8寸触摸屏）
 * 操作系统：openvela (NuttX)
 *
 * 单板可独立运行：触摸界面、用药提醒、板上传感器、提示音、本地持久化
 * 全部不依赖手环和云端。手环接入后只需往事件系统里发事件即可。
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>

#include "include/storage.h"
#include "include/sensors.h"
#include "include/audio.h"
#include "include/event.h"
#include "include/medication.h"
#include "include/cloud.h"
#include "include/lcd.h"
#include "include/ui.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LOG_TAG  "silver_hub"
#define APP_VER  "1.1.0"

#define LOOP_INTERVAL_US 20000   /* 主循环 20ms */

/****************************************************************************
 * Private Data
 ****************************************************************************/

static volatile bool g_running = true;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/**
 * @brief 依次初始化各模块
 *
 * 顺序有讲究：
 *   storage -> medication   用药计划要从 /data 读
 *   audio   -> lcd          "关于"页要读音频自检状态
 *   sensors -> lcd          "健康数据"页要读传感器可用性
 * 各模块失败都不致命，只记日志继续跑——少一个传感器不该让整机起不来。
 */

static int system_init(void)
{
  int ret;
  int opened;

  syslog(LOG_INFO, "[%s] 银发守护 Hub 启动, 版本 %s\n", LOG_TAG, APP_VER);

  /* 持久化：失败只意味着设置不落盘 */

  if (storage_init() != OK)
    {
      syslog(LOG_WARNING, "[%s] 持久化不可用，设置与用药计划只在内存里\n",
             LOG_TAG);
    }

  /* 事件系统 */

  ret = event_init();
  if (ret < 0)
    {
      syslog(LOG_ERR, "[%s] 事件系统初始化失败: %d\n", LOG_TAG, ret);
      return ret;
    }

  /* 音频：失败不阻塞，界面会显示"不可用" */

  ret = audio_init();
  if (ret < 0)
    {
      syslog(LOG_WARNING, "[%s] 音频不可用(%d)，将没有提示音\n",
             LOG_TAG, ret);
    }

  /* 板上传感器 */

  opened = sensors_init();
  syslog(LOG_INFO, "[%s] 传感器通道 %d/%d\n",
         LOG_TAG, opened, SG_SENSOR_COUNT);

  /* 用药提醒（会尝试从 /data 恢复计划） */

  ret = medication_init();
  if (ret < 0)
    {
      syslog(LOG_ERR, "[%s] 用药模块初始化失败: %d\n", LOG_TAG, ret);
      return ret;
    }

  /* 云端：当前是本地桩，只维护状态结构 */

  ret = cloud_init();
  if (ret < 0)
    {
      syslog(LOG_WARNING, "[%s] 云端模块初始化失败: %d\n", LOG_TAG, ret);
    }

  /* 显示与触摸（内部会建好所有页面） */

  ret = silver_lcd_init();
  if (ret < 0)
    {
      syslog(LOG_ERR, "[%s] 显示层初始化失败: %d\n", LOG_TAG, ret);
      return ret;
    }

  syslog(LOG_INFO, "[%s] 系统初始化完成\n", LOG_TAG);
  return OK;
}

static void system_run(void)
{
  syslog(LOG_INFO, "[%s] 进入主循环\n", LOG_TAG);

  audio_play_tone(TONE_STARTUP);

  while (g_running)
    {
      const cloud_status_t *cloud;

      /* 事件分发（SOS / 久坐 / 用药 -> 提示音 + 警示层） */

      event_process();

      /* 用药到点检查 */

      medication_process();

      /* 云端状态（当前为桩，只维护状态结构） */

      cloud_process();

      /* 传感器 1Hz 采样 */

      sensors_poll(lcd_tick_ms());

      /* 驱动 LVGL：触摸、时钟、演示模式 */

      lcd_task();

      /* 页面自身需要每秒刷新的内容 */

      ui_tick(lcd_tick_ms());

      /* 同步网络状态到状态栏 */

      cloud = cloud_get_status();
      lcd_set_network(cloud->wifi_connected, cloud->wifi_signal);

      usleep(LOOP_INTERVAL_US);
    }
}

static void system_cleanup(void)
{
  syslog(LOG_INFO, "[%s] 正在退出...\n", LOG_TAG);

  lcd_deinit();
  cloud_deinit();
  medication_deinit();   /* 内部会保存用药计划 */
  sensors_deinit();
  audio_deinit();
  event_deinit();

  syslog(LOG_INFO, "[%s] 已退出\n", LOG_TAG);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, char *argv[])
{
  int ret;

  (void)argc;
  (void)argv;

  ret = system_init();
  if (ret < 0)
    {
      syslog(LOG_ERR, "[%s] 系统初始化失败: %d\n", LOG_TAG, ret);
      return EXIT_FAILURE;
    }

  system_run();
  system_cleanup();

  return EXIT_SUCCESS;
}
