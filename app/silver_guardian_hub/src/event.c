/****************************************************************************
 * Silver Guardian Hub - 事件处理模块实现
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <sys/time.h>

#include "include/event.h"
#include "include/audio.h"
#include "include/cloud.h"
#include "include/lcd.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LOG_TAG "event"
#define EVENT_QUEUE_SIZE 32

/****************************************************************************
 * Private Data
 ****************************************************************************/

static event_t g_event_queue[EVENT_QUEUE_SIZE];
static int g_queue_head = 0;
static int g_queue_tail = 0;
static int g_queue_count = 0;

static event_handler_t g_handlers[16] = {NULL};
static bool g_initialized = false;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static uint32_t get_timestamp(void)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (uint32_t)tv.tv_sec;
}

static int queue_push(const event_t *event)
{
  if (g_queue_count >= EVENT_QUEUE_SIZE)
    {
      syslog(LOG_WARNING, "[%s] Event queue full\n", LOG_TAG);
      return -ENOSPC;
    }

  g_event_queue[g_queue_tail] = *event;
  g_queue_tail = (g_queue_tail + 1) % EVENT_QUEUE_SIZE;
  g_queue_count++;

  return OK;
}

static int queue_pop(event_t *event)
{
  if (g_queue_count == 0)
    {
      return -EAGAIN;
    }

  *event = g_event_queue[g_queue_head];
  g_queue_head = (g_queue_head + 1) % EVENT_QUEUE_SIZE;
  g_queue_count--;

  return OK;
}

/**
 * @brief 处理 SOS 事件
 */

static void handle_sos_event(const event_t *event)
{
  syslog(LOG_INFO, "[%s] Handling SOS event\n", LOG_TAG);

  /* 播放 SOS 语音 */

  audio_play("收到紧急求助，正在通知家属！");

  /* 上报云端 */

  cloud_report_alert("sos", "老人触发SOS紧急呼救");

  /* LCD 显示 SOS 界面 */

  lcd_show_sos();

  /* TODO: 启动通知流程 */
}

/**
 * @brief 处理 SOS 取消事件
 */

static void handle_sos_cancel_event(const event_t *event)
{
  syslog(LOG_INFO, "[%s] Handling SOS cancel event\n", LOG_TAG);

  audio_play("SOS 已取消");
}

/**
 * @brief 处理久坐提醒事件
 */

static void handle_sitting_event(const event_t *event)
{
  char text[128];

  syslog(LOG_INFO, "[%s] Handling sitting event\n", LOG_TAG);

  uint32_t minutes = event->param / 60;
  snprintf(text, sizeof(text),
           "您已坐了 %lu 分钟，起来活动一下吧", minutes);

  audio_play(text);

  /* LCD 显示久坐提醒 */

  lcd_show_sitting(minutes);

  cloud_report_alert("sitting_reminder", text);
}

/**
 * @brief 处理恢复活动事件
 */

static void handle_activity_resumed(const event_t *event)
{
  syslog(LOG_INFO, "[%s] Activity resumed\n", LOG_TAG);
}

/**
 * @brief 处理用药提醒事件
 */

static void handle_medication_event(const event_t *event)
{
  char text[128];

  syslog(LOG_INFO, "[%s] Handling medication event: %s\n",
         LOG_TAG, event->message);

  snprintf(text, sizeof(text),
           "该吃药了！请服用 %s", event->message);

  audio_play(text);

  /* LCD 显示用药提醒 */

  lcd_show_medication(event->message, 0, "");
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int event_init(void)
{
  syslog(LOG_INFO, "[%s] Initializing event system\n", LOG_TAG);

  memset(g_event_queue, 0, sizeof(g_event_queue));
  memset(g_handlers, 0, sizeof(g_handlers));

  /* 注册默认处理器 */

  g_handlers[EVENT_TYPE_SOS] = handle_sos_event;
  g_handlers[EVENT_TYPE_SOS_CANCEL] = handle_sos_cancel_event;
  g_handlers[EVENT_TYPE_SITTING] = handle_sitting_event;
  g_handlers[EVENT_TYPE_ACTIVITY_RESUMED] = handle_activity_resumed;
  g_handlers[EVENT_TYPE_MEDICATION] = handle_medication_event;

  g_initialized = true;

  syslog(LOG_INFO, "[%s] Event system initialized\n", LOG_TAG);

  return OK;
}

void event_deinit(void)
{
  g_initialized = false;
  syslog(LOG_INFO, "[%s] Event system deinitialized\n", LOG_TAG);
}

void event_process(void)
{
  event_t event;

  if (!g_initialized)
    {
      return;
    }

  /* 处理事件队列 */

  while (queue_pop(&event) == OK)
    {
      syslog(LOG_INFO, "[%s] Processing event: type=%d priority=%d\n",
             LOG_TAG, event.type, event.priority);

      /* 调用对应的处理器 */

      if (event.type < 16 && g_handlers[event.type] != NULL)
        {
          g_handlers[event.type](&event);
        }
      else
        {
          syslog(LOG_WARNING, "[%s] No handler for event type: %d\n",
                 LOG_TAG, event.type);
        }
    }
}

int event_send(const event_t *event)
{
  if (!g_initialized)
    {
      return -ENODEV;
    }

  syslog(LOG_INFO, "[%s] Sending event: type=%d\n",
         LOG_TAG, event->type);

  return queue_push(event);
}

void event_register_handler(uint8_t type, event_handler_t handler)
{
  if (type < 16)
    {
      g_handlers[type] = handler;
    }
}

void event_trigger_sos(void)
{
  event_t event;
  memset(&event, 0, sizeof(event_t));

  event.type = EVENT_TYPE_SOS;
  event.priority = EVENT_PRIORITY_CRITICAL;
  event.timestamp = get_timestamp();
  snprintf(event.message, sizeof(event.message), "SOS 紧急呼救");

  event_send(&event);
}

void event_trigger_sos_cancel(void)
{
  event_t event;
  memset(&event, 0, sizeof(event_t));

  event.type = EVENT_TYPE_SOS_CANCEL;
  event.priority = EVENT_PRIORITY_HIGH;
  event.timestamp = get_timestamp();

  event_send(&event);
}

void event_trigger_sitting(uint32_t duration)
{
  event_t event;
  memset(&event, 0, sizeof(event_t));

  event.type = EVENT_TYPE_SITTING;
  event.priority = EVENT_PRIORITY_MEDIUM;
  event.timestamp = get_timestamp();
  event.param = duration;
  snprintf(event.message, sizeof(event.message),
           "久坐 %lu 分钟", duration / 60);

  event_send(&event);
}

void event_trigger_activity_resumed(void)
{
  event_t event;
  memset(&event, 0, sizeof(event_t));

  event.type = EVENT_TYPE_ACTIVITY_RESUMED;
  event.priority = EVENT_PRIORITY_LOW;
  event.timestamp = get_timestamp();

  event_send(&event);
}

void event_trigger_medication(const char *drug_name)
{
  event_t event;
  memset(&event, 0, sizeof(event_t));

  event.type = EVENT_TYPE_MEDICATION;
  event.priority = EVENT_PRIORITY_MEDIUM;
  event.timestamp = get_timestamp();
  snprintf(event.message, sizeof(event.message), "%s", drug_name);

  event_send(&event);
}

int event_get_count(void)
{
  return g_queue_count;
}

void event_clear_queue(void)
{
  g_queue_head = 0;
  g_queue_tail = 0;
  g_queue_count = 0;
}
