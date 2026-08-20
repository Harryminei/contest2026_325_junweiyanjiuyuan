/****************************************************************************
 * Silver Guardian Band - UI 界面模块实现
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>
#include <sys/time.h>

#include "include/ui.h"
#include "include/imu.h"
#include "include/ble.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LOG_TAG "ui"
#define LCD_DEV "/dev/lcd0"
#define MOTOR_DEV "/dev/motor0"

/****************************************************************************
 * Private Data
 ****************************************************************************/

static int g_lcd_fd = -1;
static int g_motor_fd = -1;
static bool g_initialized = false;
static ui_screen_t g_current_screen = UI_SCREEN_MAIN;
static uint32_t g_message_timeout = 0;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static uint32_t get_tick_sec(void)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (uint32_t)tv.tv_sec;
}

static int lcd_draw_text(int x, int y, const char *text, int font_size)
{
  /* TODO: 实际的 LCD 绘制代码 */

  syslog(LOG_DEBUG, "[%s] Draw text: (%d,%d) '%s'\n",
         LOG_TAG, x, y, text);

  return OK;
}

static int lcd_clear(void)
{
  /* TODO: 实际的清屏代码 */

  return OK;
}

static int motor_control(uint8_t pattern)
{
  if (g_motor_fd < 0)
    {
      return -ENODEV;
    }

  /* TODO: 实际的马达控制代码 */

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int ui_init(void)
{
  syslog(LOG_INFO, "[%s] Initializing UI module\n", LOG_TAG);

  /* 打开 LCD 设备 */

  g_lcd_fd = open(LCD_DEV, O_WRONLY);
  if (g_lcd_fd < 0)
    {
      syslog(LOG_WARNING, "[%s] LCD not available, using console\n",
             LOG_TAG);
    }

  /* 打开马达设备 */

  g_motor_fd = open(MOTOR_DEV, O_WRONLY);
  if (g_motor_fd < 0)
    {
      syslog(LOG_WARNING, "[%s] Motor not available\n", LOG_TAG);
    }

  g_initialized = true;

  syslog(LOG_INFO, "[%s] UI module initialized\n", LOG_TAG);

  return OK;
}

void ui_deinit(void)
{
  if (g_lcd_fd >= 0)
    {
      close(g_lcd_fd);
      g_lcd_fd = -1;
    }

  if (g_motor_fd >= 0)
    {
      close(g_motor_fd);
      g_motor_fd = -1;
    }

  g_initialized = false;

  syslog(LOG_INFO, "[%s] UI module deinitialized\n", LOG_TAG);
}

void ui_process_events(void)
{
  if (!g_initialized)
    {
      return;
    }

  /* 检查消息超时 */

  if (g_message_timeout > 0 && get_tick_sec() >= g_message_timeout)
    {
      g_message_timeout = 0;
      ui_show_main_screen();
    }

  /* 处理 LVGL 事件 */

  /* TODO: LVGL 事件处理 */

  lv_timer_handler();
}

void ui_show_main_screen(void)
{
  g_current_screen = UI_SCREEN_MAIN;

  lcd_clear();

  /* 显示时间 */

  ui_update_time();

  /* 显示步数 */

  const activity_status_t *status = imu_get_activity_status();
  ui_update_steps(status->total_steps);

  /* 显示电池 */

  ui_update_battery(ble_get_battery_level());

  /* 显示 BLE 状态 */

  ui_update_ble_status(ble_is_connected());

  syslog(LOG_INFO, "[%s] Show main screen\n", LOG_TAG);
}

void ui_show_sos_screen(void)
{
  g_current_screen = UI_SCREEN_SOS;

  lcd_clear();

  /* 显示 SOS 界面 */

  lcd_draw_text(60, 80, "⚠️ SOS ⚠️", FONT_SIZE_LARGE);
  lcd_draw_text(40, 130, "已发送求助", FONT_SIZE_MEDIUM);
  lcd_draw_text(50, 170, "等待响应...", FONT_SIZE_SMALL);

  syslog(LOG_INFO, "[%s] Show SOS screen\n", LOG_TAG);
}

void ui_show_sos_cancel_prompt(void)
{
  g_current_screen = UI_SCREEN_SOS_CANCEL;

  lcd_clear();

  lcd_draw_text(40, 80, "是否取消 SOS？", FONT_SIZE_MEDIUM);
  lcd_draw_text(50, 130, "按任意键取消", FONT_SIZE_SMALL);

  syslog(LOG_INFO, "[%s] Show SOS cancel prompt\n", LOG_TAG);
}

void ui_show_sitting_reminder(uint32_t duration)
{
  g_current_screen = UI_SCREEN_SITTING;

  lcd_clear();

  char text[64];
  snprintf(text, sizeof(text), "久坐 %lu 分钟", duration / 60);

  lcd_draw_text(30, 80, "🪑 久坐提醒", FONT_SIZE_LARGE);
  lcd_draw_text(40, 130, text, FONT_SIZE_MEDIUM);
  lcd_draw_text(30, 170, "起来活动一下吧", FONT_SIZE_SMALL);

  syslog(LOG_INFO, "[%s] Show sitting reminder\n", LOG_TAG);
}

void ui_show_alert_screen(const char *message)
{
  g_current_screen = UI_SCREEN_ALERT;

  lcd_clear();

  lcd_draw_text(40, 80, "⚠️ 告警", FONT_SIZE_LARGE);
  lcd_draw_text(20, 130, message, FONT_SIZE_SMALL);

  syslog(LOG_INFO, "[%s] Show alert: %s\n", LOG_TAG, message);
}

void ui_update_time(void)
{
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  char time_str[16];

  snprintf(time_str, sizeof(time_str), "%02d:%02d",
           tm->tm_hour, tm->tm_min);

  lcd_draw_text(70, 30, time_str, FONT_SIZE_LARGE);
}

void ui_update_steps(uint32_t steps)
{
  char text[32];
  snprintf(text, sizeof(text), "步数: %lu", steps);

  lcd_draw_text(60, 190, text, FONT_SIZE_SMALL);
}

void ui_update_battery(uint8_t level)
{
  char text[16];
  snprintf(text, sizeof(text), "电量: %d%%", level);

  lcd_draw_text(160, 30, text, FONT_SIZE_SMALL);
}

void ui_update_ble_status(bool connected)
{
  if (connected)
    {
      lcd_draw_text(10, 30, "BLE ✓", FONT_SIZE_SMALL);
    }
  else
    {
      lcd_draw_text(10, 30, "BLE ✗", FONT_SIZE_SMALL);
    }
}

void ui_show_message(const char *message, uint32_t duration)
{
  lcd_clear();
  lcd_draw_text(20, 100, message, FONT_SIZE_MEDIUM);

  g_message_timeout = get_tick_sec() + duration;

  syslog(LOG_INFO, "[%s] Show message: %s\n", LOG_TAG, message);
}

void ui_clear_screen(void)
{
  lcd_clear();
}

void ui_refresh(void)
{
  /* 刷新显示 */

  /* TODO: 实际的刷新代码 */
}

/****************************************************************************
 * Motor Control Functions
 ****************************************************************************/

void motor_vibrate_short(void)
{
  motor_control(1);  /* 短震动模式 */
}

void motor_vibrate_long(void)
{
  motor_control(2);  /* 长震动模式 */
}

void motor_vibrate_sos(void)
{
  motor_control(3);  /* SOS 模式：三短三长三短 */
}

void motor_stop(void)
{
  motor_control(0);  /* 停止 */
}
