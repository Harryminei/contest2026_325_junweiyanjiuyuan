/****************************************************************************
 * Silver Guardian Hub - 事件处理模块实现
 *
 * 事件从队列里取出后分发给对应处理器，同时记一条历史（环形缓冲），
 * 供"事件记录"页展示最近发生过什么。
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
#include "include/led.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LOG_TAG "event"
#define EVENT_QUEUE_SIZE 32

/****************************************************************************
 * Private Data
 ****************************************************************************/

static event_t g_event_queue[EVENT_QUEUE_SIZE];
static int g_queue_head;
static int g_queue_tail;
static int g_queue_count;

static event_handler_t g_handlers[16];

/* 历史：环形缓冲，g_history_next 指向下一个要写的位置 */

static event_t g_history[EVENT_HISTORY_SIZE];
static int g_history_next;
static int g_history_count;

static bool g_initialized;

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
      syslog(LOG_WARNING, "[%s] 事件队列已满，丢弃 type=%d\n",
             LOG_TAG, event->type);
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

static void history_push(const event_t *event)
{
  g_history[g_history_next] = *event;
  g_history_next = (g_history_next + 1) % EVENT_HISTORY_SIZE;

  if (g_history_count < EVENT_HISTORY_SIZE)
    {
      g_history_count++;
    }
}

/*--------------------------------------------------------------------------
 * 各类型事件的处理器
 *------------------------------------------------------------------------*/

static void handle_sos_event(const event_t *event)
{
  (void)event;

  syslog(LOG_INFO, "[%s] 处理 SOS 事件\n", LOG_TAG);

  audio_play_tone(TONE_SOS);
  sg_led_alert(SG_LED_COLOR_RED, SG_LED_BLINK_FAST, SG_LED_SOS_HOLD_MS);
  cloud_report_alert("sos", "老人触发SOS紧急呼救");

  lcd_show_alert(LCD_ALERT_SOS, "紧急求助！",
                 "SOS 已发出\n正在通知家属...\n\n请保持冷静");
}

static void handle_sos_cancel_event(const event_t *event)
{
  (void)event;

  syslog(LOG_INFO, "[%s] 处理 SOS 取消\n", LOG_TAG);

  audio_play_tone(TONE_CLICK);
  sg_led_off();
  lcd_clear_alert();
}

static void handle_sitting_event(const event_t *event)
{
  char body[128];
  uint32_t minutes = event->param / 60;

  syslog(LOG_INFO, "[%s] 处理久坐事件 (%u 分钟)\n", LOG_TAG,
         (unsigned)minutes);

  snprintf(body, sizeof(body), "您已静坐 %lu 分钟\n请起身活动一下",
           (unsigned long)minutes);

  audio_play_tone(TONE_SITTING);
  sg_led_alert(SG_LED_COLOR_YELLOW, SG_LED_BLINK_SLOW, SG_LED_NOTIFY_HOLD_MS);
  lcd_show_alert(LCD_ALERT_SITTING, "久坐提醒", body);

  cloud_report_alert("sitting_reminder", body);
}

static void handle_activity_resumed(const event_t *event)
{
  (void)event;

  syslog(LOG_INFO, "[%s] 恢复活动\n", LOG_TAG);

  /* 人回来了，告警灯没有继续闪的理由 */

  sg_led_off();
}

static void handle_medication_event(const event_t *event)
{
  char body[128];

  syslog(LOG_INFO, "[%s] 处理用药事件: %s\n", LOG_TAG, event->message);

  snprintf(body, sizeof(body), "该吃药了\n\n请服用 %s", event->message);

  audio_play_tone(TONE_MEDICATION);
  sg_led_alert(SG_LED_COLOR_GREEN, SG_LED_BLINK_SLOW, SG_LED_NOTIFY_HOLD_MS);
  lcd_show_alert(LCD_ALERT_MEDICATION, "用药提醒", body);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

const char *event_type_name(uint8_t type)
{
  switch (type)
    {
      case EVENT_TYPE_SOS:              return "紧急求助";
      case EVENT_TYPE_SOS_CANCEL:       return "求助取消";
      case EVENT_TYPE_SITTING:          return "久坐提醒";
      case EVENT_TYPE_STILL_ALERT:      return "静止告警";
      case EVENT_TYPE_ACTIVITY_RESUMED: return "恢复活动";
      case EVENT_TYPE_MEDICATION:       return "用药提醒";
      case EVENT_TYPE_CLOUD_CMD:        return "云端指令";
      default:                          return "其他";
    }
}

int event_init(void)
{
  syslog(LOG_INFO, "[%s] 初始化事件系统\n", LOG_TAG);

  memset(g_event_queue, 0, sizeof(g_event_queue));
  memset(g_history, 0, sizeof(g_history));
  memset(g_handlers, 0, sizeof(g_handlers));

  g_queue_head    = 0;
  g_queue_tail    = 0;
  g_queue_count   = 0;
  g_history_next  = 0;
  g_history_count = 0;

  g_handlers[EVENT_TYPE_SOS]              = handle_sos_event;
  g_handlers[EVENT_TYPE_SOS_CANCEL]       = handle_sos_cancel_event;
  g_handlers[EVENT_TYPE_SITTING]          = handle_sitting_event;
  g_handlers[EVENT_TYPE_ACTIVITY_RESUMED] = handle_activity_resumed;
  g_handlers[EVENT_TYPE_MEDICATION]       = handle_medication_event;

  g_initialized = true;
  return OK;
}

void event_deinit(void)
{
  g_initialized = false;
  syslog(LOG_INFO, "[%s] 事件系统已关闭\n", LOG_TAG);
}

void event_process(void)
{
  event_t event;

  if (!g_initialized)
    {
      return;
    }

  while (queue_pop(&event) == OK)
    {
      syslog(LOG_INFO, "[%s] 分发事件 type=%d priority=%d\n",
             LOG_TAG, event.type, event.priority);

      history_push(&event);

      if (event.type < 16 && g_handlers[event.type] != NULL)
        {
          g_handlers[event.type](&event);
        }
      else
        {
          syslog(LOG_WARNING, "[%s] 事件类型 %d 没有处理器\n",
                 LOG_TAG, event.type);
        }
    }
}

int event_send(const event_t *event)
{
  if (!g_initialized || event == NULL)
    {
      return -ENODEV;
    }

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

  memset(&event, 0, sizeof(event));
  event.type      = EVENT_TYPE_SOS;
  event.priority  = EVENT_PRIORITY_CRITICAL;
  event.timestamp = get_timestamp();
  snprintf(event.message, sizeof(event.message), "SOS 紧急呼救");

  event_send(&event);
}

void event_trigger_sos_cancel(void)
{
  event_t event;

  memset(&event, 0, sizeof(event));
  event.type      = EVENT_TYPE_SOS_CANCEL;
  event.priority  = EVENT_PRIORITY_HIGH;
  event.timestamp = get_timestamp();

  event_send(&event);
}

void event_trigger_sitting(uint32_t duration)
{
  event_t event;

  memset(&event, 0, sizeof(event));
  event.type      = EVENT_TYPE_SITTING;
  event.priority  = EVENT_PRIORITY_MEDIUM;
  event.timestamp = get_timestamp();
  event.param     = duration;
  snprintf(event.message, sizeof(event.message),
           "久坐 %lu 分钟", (unsigned long)(duration / 60));

  event_send(&event);
}

void event_trigger_activity_resumed(void)
{
  event_t event;

  memset(&event, 0, sizeof(event));
  event.type      = EVENT_TYPE_ACTIVITY_RESUMED;
  event.priority  = EVENT_PRIORITY_LOW;
  event.timestamp = get_timestamp();

  event_send(&event);
}

void event_trigger_medication(const char *drug_name)
{
  event_t event;

  memset(&event, 0, sizeof(event));
  event.type      = EVENT_TYPE_MEDICATION;
  event.priority  = EVENT_PRIORITY_MEDIUM;
  event.timestamp = get_timestamp();
  snprintf(event.message, sizeof(event.message), "%s",
           drug_name ? drug_name : "药物");

  event_send(&event);
}

int event_get_count(void)
{
  return g_queue_count;
}

void event_clear_queue(void)
{
  g_queue_head  = 0;
  g_queue_tail  = 0;
  g_queue_count = 0;
}

int event_get_history(event_t *out, int max_count)
{
  int i;
  int count;

  if (out == NULL || max_count <= 0)
    {
      return 0;
    }

  count = (g_history_count < max_count) ? g_history_count : max_count;

  /* 最新的在前：从 g_history_next 往回取 */

  for (i = 0; i < count; i++)
    {
      int idx = (g_history_next - 1 - i + EVENT_HISTORY_SIZE * 2)
                % EVENT_HISTORY_SIZE;
      out[i] = g_history[idx];
    }

  return count;
}

void event_clear_history(void)
{
  memset(g_history, 0, sizeof(g_history));
  g_history_next  = 0;
  g_history_count = 0;
}
