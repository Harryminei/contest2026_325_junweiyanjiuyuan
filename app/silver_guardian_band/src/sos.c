/****************************************************************************
 * Silver Guardian Band - SOS 呼救模块实现
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
#include <nuttx/ioexpander/gpio.h>
#include <sys/time.h>

#include "include/sos.h"
#include "include/ble.h"
#include "include/ui.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LOG_TAG "sos"

/****************************************************************************
 * Private Data
 ****************************************************************************/

static sos_status_t g_sos_status;
static sos_event_callback_t g_callback = NULL;
static int g_gpio_fd = -1;
static bool g_initialized = false;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/**
 * @brief 获取当前时间（毫秒）
 */

static uint32_t get_tick_ms(void)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (uint32_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

/**
 * @brief 读取按键状态
 * @return true 按下，false 释放
 */

static bool read_button(void)
{
  bool value = false;

  if (g_gpio_fd >= 0)
    {
      struct gpio_pin_read_s pin_read;
      pin_read.pin = SOS_BUTTON_PIN;

      if (ioctl(g_gpio_fd, GPIOC_PINREAD, (unsigned long)&pin_read) >= 0)
        {
          value = !pin_read.value; /* 低电平有效 */
        }
    }

  return value;
}

/**
 * @brief 触发事件通知
 */

static void notify_event(sos_event_t event)
{
  if (g_callback != NULL)
    {
      g_callback(event);
    }
}

/**
 * @brief 发送 SOS 到 Hub
 */

static int send_sos_to_hub(void)
{
  int ret;

  syslog(LOG_INFO, "[%s] Sending SOS to Hub...\n", LOG_TAG);

  /* 通过 BLE 发送 SOS 告警 */

  ret = ble_send_sos_alert();
  if (ret < 0)
    {
      syslog(LOG_ERR, "[%s] Failed to send SOS via BLE: %d\n",
             LOG_TAG, ret);
      return ret;
    }

  g_sos_status.is_sent = true;

  syslog(LOG_INFO, "[%s] SOS sent successfully\n", LOG_TAG);

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int sos_init(void)
{
  syslog(LOG_INFO, "[%s] Initializing SOS module\n", LOG_TAG);

  /* 初始化状态 */

  memset(&g_sos_status, 0, sizeof(sos_status_t));
  g_sos_status.state = SOS_STATE_IDLE;

  /* 打开 GPIO 设备 */

  g_gpio_fd = open("/dev/gpio0", O_RDONLY);
  if (g_gpio_fd < 0)
    {
      syslog(LOG_ERR, "[%s] Failed to open GPIO: %d\n",
             LOG_TAG, errno);
      return -errno;
    }

  /* 配置 GPIO 为输入，上拉，下降沿中断 */

  struct gpio_pin_config_s config;
  config.pin = SOS_BUTTON_PIN;
  config.mode = GPIO_INPUT_PULL_UP;

  if (ioctl(g_gpio_fd, GPIOC_SETPINCONFIG, (unsigned long)&config) < 0)
    {
      syslog(LOG_ERR, "[%s] Failed to config GPIO: %d\n",
             LOG_TAG, errno);
      close(g_gpio_fd);
      g_gpio_fd = -1;
      return -errno;
    }

  g_initialized = true;

  syslog(LOG_INFO, "[%s] SOS module initialized\n", LOG_TAG);

  return OK;
}

void sos_deinit(void)
{
  if (g_gpio_fd >= 0)
    {
      close(g_gpio_fd);
      g_gpio_fd = -1;
    }

  g_initialized = false;

  syslog(LOG_INFO, "[%s] SOS module deinitialized\n", LOG_TAG);
}

void sos_check(void)
{
  bool pressed;
  uint32_t now;
  uint32_t duration;

  if (!g_initialized)
    {
      return;
    }

  pressed = read_button();
  now = get_tick_ms();

  switch (g_sos_status.state)
    {
      case SOS_STATE_IDLE:
        if (pressed)
          {
            /* 按键按下，开始计时 */

            g_sos_status.state = SOS_STATE_PRESSING;
            g_sos_status.press_start = now;

            syslog(LOG_INFO, "[%s] Button pressed, timing started\n",
                   LOG_TAG);

            /* 震动反馈 */

            motor_vibrate_short();
          }
        break;

      case SOS_STATE_PRESSING:
        if (!pressed)
          {
            /* 按键释放，重置状态 */

            g_sos_status.state = SOS_STATE_IDLE;

            syslog(LOG_INFO, "[%s] Button released, reset\n", LOG_TAG);
          }
        else
          {
            duration = now - g_sos_status.press_start;

            if (duration >= SOS_PRESS_DURATION_MS)
              {
                /* 长按时间达到，触发 SOS */

                g_sos_status.state = SOS_STATE_TRIGGERED;
                g_sos_status.is_triggered = true;
                g_sos_status.trigger_time = now;

                syslog(LOG_INFO, "[%s] SOS triggered!\n", LOG_TAG);

                /* 发送 SOS */

                send_sos_to_hub();

                /* 通知事件 */

                notify_event(SOS_EVENT_TRIGGERED);

                /* 震动提醒 */

                motor_vibrate_sos();
              }
          }
        break;

      case SOS_STATE_TRIGGERED:
        /* 检查取消超时 */

        duration = now - g_sos_status.trigger_time;

        if (duration < SOS_CANCEL_TIMEOUT_MS)
          {
            /* 在取消窗口内 */

            g_sos_status.state = SOS_STATE_CANCELLING;

            /* 显示取消提示 */

            ui_show_sos_cancel_prompt();
          }
        else
          {
            /* 取消窗口已过 */

            notify_event(SOS_EVENT_TIMEOUT);

            /* 保持触发状态 */

            g_sos_status.state = SOS_STATE_IDLE;
            g_sos_status.is_triggered = false;
          }
        break;

      case SOS_STATE_CANCELLING:
        if (pressed)
          {
            /* 按下取消 */

            sos_cancel();
          }

        /* 检查超时 */

        duration = now - g_sos_status.trigger_time;
        if (duration >= SOS_CANCEL_TIMEOUT_MS)
          {
            /* 超时，发送最终确认 */

            notify_event(SOS_EVENT_SENT);

            g_sos_status.state = SOS_STATE_IDLE;
            g_sos_status.is_triggered = false;
          }
        break;

      default:
        g_sos_status.state = SOS_STATE_IDLE;
        break;
    }
}

const sos_status_t *sos_get_status(void)
{
  return &g_sos_status;
}

int sos_trigger_manual(void)
{
  if (!g_initialized)
    {
      return -ENODEV;
    }

  syslog(LOG_INFO, "[%s] Manual SOS trigger\n", LOG_TAG);

  g_sos_status.state = SOS_STATE_TRIGGERED;
  g_sos_status.is_triggered = true;
  g_sos_status.trigger_time = get_tick_ms();

  send_sos_to_hub();

  notify_event(SOS_EVENT_TRIGGERED);

  motor_vibrate_sos();

  return OK;
}

int sos_cancel(void)
{
  if (!g_sos_status.is_triggered)
    {
      return -EPERM;
    }

  uint32_t now = get_tick_ms();
  uint32_t duration = now - g_sos_status.trigger_time;

  if (duration >= SOS_CANCEL_TIMEOUT_MS)
    {
      syslog(LOG_WARNING, "[%s] Cancel timeout\n", LOG_TAG);
      return -ETIMEDOUT;
    }

  syslog(LOG_INFO, "[%s] SOS cancelled\n", LOG_TAG);

  g_sos_status.state = SOS_STATE_IDLE;
  g_sos_status.is_triggered = false;
  g_sos_status.is_sent = false;

  /* 通知 Hub 取消 */

  ble_send_sos_cancel();

  notify_event(SOS_EVENT_CANCELLED);

  return OK;
}

void sos_register_callback(sos_event_callback_t callback)
{
  g_callback = callback;
}

bool sos_is_triggered(void)
{
  return g_sos_status.is_triggered;
}

void sos_reset(void)
{
  memset(&g_sos_status, 0, sizeof(sos_status_t));
  g_sos_status.state = SOS_STATE_IDLE;
}
