/****************************************************************************
 * Silver Guardian Hub - 用药提醒模块实现
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

#include "include/medication.h"
#include "include/event.h"
#include "include/audio.h"
#include "include/cloud.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LOG_TAG "medication"

/****************************************************************************
 * Private Data
 ****************************************************************************/

static medication_plan_t g_plans[MAX_MEDICATIONS];
static int g_plan_count = 0;
static medication_record_t g_records[100];
static int g_record_count = 0;
static bool g_initialized = false;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static uint32_t get_time_sec(void)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (uint32_t)tv.tv_sec;
}

static int get_current_hour(void)
{
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  return tm->tm_hour;
}

static int get_current_minute(void)
{
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  return tm->tm_min;
}

static void save_record(int plan_index)
{
  if (g_record_count >= 100)
    {
      /* 移动记录 */

      memmove(&g_records[0], &g_records[1],
              sizeof(medication_record_t) * 99);
      g_record_count = 99;
    }

  medication_record_t *record = &g_records[g_record_count];
  strncpy(record->name, g_plans[plan_index].name,
          MEDICATION_NAME_LEN);
  record->scheduled_time = get_time_sec();
  record->taken_time = 0;
  record->taken = false;

  g_record_count++;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int medication_init(void)
{
  syslog(LOG_INFO, "[%s] Initializing medication system\n", LOG_TAG);

  memset(g_plans, 0, sizeof(g_plans));
  memset(g_records, 0, sizeof(g_records));

  /* 添加示例用药计划（用于测试） */

  medication_plan_t demo;
  memset(&demo, 0, sizeof(demo));
  strncpy(demo.name, "降压药", MEDICATION_NAME_LEN);
  demo.hour = 8;
  demo.minute = 0;
  demo.dosage = 1;
  strncpy(demo.unit, "片", 8);
  strncpy(demo.notes, "饭后服用", MEDICATION_NOTES_LEN);
  demo.enabled = true;

  medication_add_plan(&demo);

  g_initialized = true;

  syslog(LOG_INFO, "[%s] Medication system initialized\n", LOG_TAG);

  return OK;
}

void medication_deinit(void)
{
  g_initialized = false;
  syslog(LOG_INFO, "[%s] Medication system deinitialized\n", LOG_TAG);
}

void medication_process(void)
{
  if (!g_initialized)
    {
      return;
    }

  int hour = get_current_hour();
  int minute = get_current_minute();
  uint32_t now = get_time_sec();

  for (int i = 0; i < g_plan_count; i++)
    {
      medication_plan_t *plan = &g_plans[i];

      if (!plan->enabled || plan->taken)
        {
          continue;
        }

      /* 检查是否到达提醒时间 */

      if (plan->hour == hour && plan->minute == minute)
        {
          /* 检查是否需要提醒 */

          if (plan->remind_count == 0 ||
              (now - plan->last_remind_time >= REMIND_INTERVAL_SEC &&
               plan->remind_count < MAX_REMIND_TIMES))
            {
              /* 触发用药提醒 */

              event_trigger_medication(plan->name);

              plan->remind_count++;
              plan->last_remind_time = now;

              /* 保存记录 */

              save_record(i);

              syslog(LOG_INFO, "[%s] Medication reminder: %s\n",
                     LOG_TAG, plan->name);
            }
          else if (plan->remind_count >= MAX_REMIND_TIMES)
            {
              /* 3 次提醒后通知家属 */

              syslog(LOG_WARNING, "[%s] Medication missed: %s\n",
                     LOG_TAG, plan->name);

              /* 通知家属：用药超时未服，上送云端告警 */

              cloud_report_alert("medication_missed", plan->name);

              plan->taken = true;  /* 标记为已处理 */
            }
        }

      /* 检查是否跨天，重置状态 */

      if (hour == 0 && minute == 0)
        {
          plan->taken = false;
          plan->remind_count = 0;
        }
    }
}

int medication_add_plan(const medication_plan_t *plan)
{
  if (g_plan_count >= MAX_MEDICATIONS)
    {
      return -ENOSPC;
    }

  g_plans[g_plan_count] = *plan;
  g_plan_count++;

  syslog(LOG_INFO, "[%s] Added medication plan: %s\n",
         LOG_TAG, plan->name);

  return OK;
}

int medication_remove_plan(int index)
{
  if (index < 0 || index >= g_plan_count)
    {
      return -EINVAL;
    }

  /* 移动后续计划 */

  for (int i = index; i < g_plan_count - 1; i++)
    {
      g_plans[i] = g_plans[i + 1];
    }

  g_plan_count--;

  syslog(LOG_INFO, "[%s] Removed medication plan: %d\n",
         LOG_TAG, index);

  return OK;
}

const medication_plan_t *medication_get_plan(int index)
{
  if (index < 0 || index >= g_plan_count)
    {
      return NULL;
    }

  return &g_plans[index];
}

int medication_get_plan_count(void)
{
  return g_plan_count;
}

int medication_confirm_taken(int index)
{
  if (index < 0 || index >= g_plan_count)
    {
      return -EINVAL;
    }

  g_plans[index].taken = true;
  g_plans[index].remind_count = 0;

  /* 更新记录 */

  if (g_record_count > 0)
    {
      g_records[g_record_count - 1].taken = true;
      g_records[g_record_count - 1].taken_time = get_time_sec();
    }

  syslog(LOG_INFO, "[%s] Medication confirmed: %s\n",
         LOG_TAG, g_plans[index].name);

  return OK;
}

int medication_snooze(int index)
{
  if (index < 0 || index >= g_plan_count)
    {
      return -EINVAL;
    }

  /* 重置提醒计数，5分钟后再次提醒 */

  g_plans[index].remind_count = MAX_REMIND_TIMES - 1;
  g_plans[index].last_remind_time = get_time_sec();

  syslog(LOG_INFO, "[%s] Medication snoozed: %s\n",
         LOG_TAG, g_plans[index].name);

  return OK;
}

bool medication_has_pending(void)
{
  for (int i = 0; i < g_plan_count; i++)
    {
      if (g_plans[i].enabled && !g_plans[i].taken)
        {
          return true;
        }
    }

  return false;
}

int medication_get_records(medication_record_t *records,
                           int max_count)
{
  int count = (g_record_count < max_count) ?
              g_record_count : max_count;

  memcpy(records, g_records,
         count * sizeof(medication_record_t));

  return count;
}

int medication_sync_from_cloud(void)
{
  syslog(LOG_INFO, "[%s] Syncing medication from cloud\n", LOG_TAG);

  /* TODO: 从云端同步用药计划 */

  return OK;
}
