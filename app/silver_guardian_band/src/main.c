/****************************************************************************
 * Silver Guardian Band - 手环端主程序
 *
 * 银发守护系统 - 手环端核心功能
 * 硬件平台：黄山派 SF32LB52
 * 操作系统：openvela (NuttX)
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>

#include "include/sos.h"
#include "include/imu.h"
#include "include/ble.h"
#include "include/ui.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LOG_TAG "silver_guardian"
#define LOG_LVL LOG_INFO

/****************************************************************************
 * Private Data
 ****************************************************************************/

static bool g_running = true;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: system_init
 *
 * Description:
 *   初始化所有子系统
 *
 ****************************************************************************/

static int system_init(void)
{
  int ret;

  syslog(LOG_INFO, "[%s] Initializing system...\n", LOG_TAG);

  /* 初始化 UI */

  ret = ui_init();
  if (ret < 0)
    {
      syslog(LOG_ERR, "[%s] UI init failed: %d\n", LOG_TAG, ret);
      return ret;
    }

  syslog(LOG_INFO, "[%s] UI initialized\n", LOG_TAG);

  /* 初始化 IMU */

  ret = imu_init();
  if (ret < 0)
    {
      syslog(LOG_ERR, "[%s] IMU init failed: %d\n", LOG_TAG, ret);
      return ret;
    }

  syslog(LOG_INFO, "[%s] IMU initialized\n", LOG_TAG);

  /* 初始化 BLE */

  ret = ble_init();
  if (ret < 0)
    {
      syslog(LOG_ERR, "[%s] BLE init failed: %d\n", LOG_TAG, ret);
      return ret;
    }

  syslog(LOG_INFO, "[%s] BLE initialized\n", LOG_TAG);

  /* 初始化 SOS */

  ret = sos_init();
  if (ret < 0)
    {
      syslog(LOG_ERR, "[%s] SOS init failed: %d\n", LOG_TAG, ret);
      return ret;
    }

  syslog(LOG_INFO, "[%s] SOS initialized\n", LOG_TAG);

  syslog(LOG_INFO, "[%s] System initialized successfully\n", LOG_TAG);

  return OK;
}

/****************************************************************************
 * Name: system_run
 *
 * Description:
 *   主循环
 *
 ****************************************************************************/

static void system_run(void)
{
  syslog(LOG_INFO, "[%s] Starting main loop\n", LOG_TAG);

  /* 显示主界面 */

  ui_show_main_screen();

  /* 主循环 */

  while (g_running)
    {
      /* 处理 UI 事件 */

      ui_process_events();

      /* 更新 IMU 数据 */

      imu_update();

      /* 检查 SOS 状态 */

      sos_check();

      /* BLE 保持连接 */

      ble_process();

      /* 休眠 10ms */

      usleep(10000);
    }
}

/****************************************************************************
 * Name: system_cleanup
 *
 * Description:
 *   清理资源
 *
 ****************************************************************************/

static void system_cleanup(void)
{
  syslog(LOG_INFO, "[%s] Cleaning up...\n", LOG_TAG);

  sos_deinit();
  ble_deinit();
  imu_deinit();
  ui_deinit();

  syslog(LOG_INFO, "[%s] Cleanup complete\n", LOG_TAG);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: main
 *
 * Description:
 *   手环端主入口
 *
 ****************************************************************************/

int main(int argc, char *argv[])
{
  int ret;

  syslog(LOG_INFO, "[%s] Silver Guardian Band starting...\n", LOG_TAG);
  syslog(LOG_INFO, "[%s] Version: 1.0.0\n", LOG_TAG);

  /* 初始化系统 */

  ret = system_init();
  if (ret < 0)
    {
      syslog(LOG_ERR, "[%s] System init failed: %d\n", LOG_TAG, ret);
      return EXIT_FAILURE;
    }

  /* 运行主循环 */

  system_run();

  /* 清理资源 */

  system_cleanup();

  syslog(LOG_INFO, "[%s] Silver Guardian Band stopped\n", LOG_TAG);

  return EXIT_SUCCESS;
}
