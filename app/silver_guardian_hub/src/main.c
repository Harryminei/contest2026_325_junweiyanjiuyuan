/****************************************************************************
 * Silver Guardian Hub - Hub端主程序
 *
 * 银发守护系统 - Hub端核心功能
 * 硬件平台：润芯微 Gemini-S1
 * 操作系统：openvela (NuttX)
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

#include "include/event.h"
#include "include/audio.h"
#include "include/medication.h"
#include "include/cloud.h"
#include "include/lcd.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LOG_TAG "silver_hub"
#define LOG_LVL LOG_INFO

/****************************************************************************
 * Private Data
 ****************************************************************************/

static bool g_running = true;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int system_init(void)
{
  int ret;

  syslog(LOG_INFO, "[%s] Initializing Hub system...\n", LOG_TAG);

  /* 初始化事件系统 */

  ret = event_init();
  if (ret < 0)
    {
      syslog(LOG_ERR, "[%s] Event init failed: %d\n", LOG_TAG, ret);
      return ret;
    }

  syslog(LOG_INFO, "[%s] Event system initialized\n", LOG_TAG);

  /* 初始化音频系统 */

  ret = audio_init();
  if (ret < 0)
    {
      syslog(LOG_ERR, "[%s] Audio init failed: %d\n", LOG_TAG, ret);
      return ret;
    }

  syslog(LOG_INFO, "[%s] Audio system initialized\n", LOG_TAG);

  /* 初始化用药提醒 */

  ret = medication_init();
  if (ret < 0)
    {
      syslog(LOG_ERR, "[%s] Medication init failed: %d\n",
             LOG_TAG, ret);
      return ret;
    }

  syslog(LOG_INFO, "[%s] Medication system initialized\n", LOG_TAG);

  /* 初始化云端通信 */

  ret = cloud_init();
  if (ret < 0)
    {
      syslog(LOG_ERR, "[%s] Cloud init failed: %d\n", LOG_TAG, ret);
      return ret;
    }

  syslog(LOG_INFO, "[%s] Cloud system initialized\n", LOG_TAG);

  /* 初始化 LCD 界面 */

  ret = lcd_init();
  if (ret < 0)
    {
      syslog(LOG_ERR, "[%s] LCD init failed: %d\n", LOG_TAG, ret);
      return ret;
    }

  syslog(LOG_INFO, "[%s] LCD system initialized\n", LOG_TAG);

  syslog(LOG_INFO, "[%s] Hub system initialized successfully\n",
         LOG_TAG);

  return OK;
}

static void system_run(void)
{
  syslog(LOG_INFO, "[%s] Starting Hub main loop\n", LOG_TAG);

  /* 播放欢迎语音 */

  audio_play("银发守护已启动，祝您生活愉快！");

  while (g_running)
    {
      const cloud_status_t *cloud;

      /* 处理事件 */

      event_process();

      /* 处理用药提醒 */

      medication_process();

      /* 处理云端通信 */

      cloud_process();

      /* 同步网络状态到 LCD 并刷新界面 */

      cloud = cloud_get_status();
      lcd_set_network(cloud->wifi_connected, cloud->wifi_signal);
      lcd_task();

      /* 休眠 20ms */

      usleep(20000);
    }
}

static void system_cleanup(void)
{
  syslog(LOG_INFO, "[%s] Cleaning up Hub...\n", LOG_TAG);

  cloud_deinit();
  medication_deinit();
  audio_deinit();
  event_deinit();
  lcd_deinit();

  syslog(LOG_INFO, "[%s] Hub cleanup complete\n", LOG_TAG);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, char *argv[])
{
  int ret;

  syslog(LOG_INFO, "[%s] Silver Guardian Hub starting...\n", LOG_TAG);
  syslog(LOG_INFO, "[%s] Version: 1.0.0\n", LOG_TAG);

  ret = system_init();
  if (ret < 0)
    {
      syslog(LOG_ERR, "[%s] System init failed: %d\n", LOG_TAG, ret);
      return EXIT_FAILURE;
    }

  system_run();

  system_cleanup();

  syslog(LOG_INFO, "[%s] Silver Guardian Hub stopped\n", LOG_TAG);

  return EXIT_SUCCESS;
}
