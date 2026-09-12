/****************************************************************************
 * Silver Guardian Hub - 用药提醒模块实现
 *
 * 提醒逻辑（相比旧版重写，旧版有两个硬伤）：
 *   旧版要求 `plan->hour == hour && plan->minute == minute` —— 也就是主循环
 *   必须恰好在那一分钟内被调度到，否则当天的提醒永远不触发；漏服上报又被
 *   塞在这个分支里，实际几乎不可能执行到。
 *   跨天重置用 `hour == 0 && minute == 0` 同样要求正好卡在 00:00 那一分钟。
 *
 * 现在改成"按天记账"：
 *   - 每个计划记 last_fired_day（当天日期戳），当天到点后只触发一次；
 *   - 未确认则每 REMIND_INTERVAL_SEC 再响，最多 MAX_REMIND_TIMES 次；
 *   - 次数用尽后再过 MEDICATION_MISSED_SEC 仍未确认 -> 判为漏服并上报；
 *   - 日期戳变了就重置当天的 taken / remind_count。
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
#include "include/storage.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LOG_TAG      "medication"
#define STORE_KEY    "medication"
#define STORE_VERSION 1

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* 落盘格式：带版本号，便于以后改结构 */

typedef struct
{
  uint8_t           version;
  uint8_t           count;
  medication_plan_t plans[MAX_MEDICATIONS];
} med_store_t;

/****************************************************************************
 * Private Data
 ****************************************************************************/

static medication_plan_t   g_plans[MAX_MEDICATIONS];
static int                 g_plan_count;
static medication_record_t g_records[100];
static int                 g_record_count;
static bool                g_initialized;

/* 常用药品预设：触屏上没法打中文，新增计划时从这里挑 */

static const char *g_presets[] =
{
  "降压药", "降糖药", "钙片", "维生素",
  "阿司匹林", "他汀", "胃药", "安眠药",
  "感冒药", "护心片"
};

#define PRESET_COUNT ((int)(sizeof(g_presets) / sizeof(g_presets[0])))

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static uint32_t get_time_sec(void)
{
  struct timeval tv;

  gettimeofday(&tv, NULL);
  return (uint32_t)tv.tv_sec;
}

/**
 * @brief 当天日期戳（年月日合成一个数，用来判断跨天）
 */

static uint32_t get_day_stamp(void)
{
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);

  return (uint32_t)(tm->tm_year + 1900) * 10000u +
         (uint32_t)(tm->tm_mon + 1) * 100u +
         (uint32_t)tm->tm_mday;
}

/**
 * @brief 当前时刻在一天中的分钟数
 */

static int get_minute_of_day(void)
{
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);

  return tm->tm_hour * 60 + tm->tm_min;
}

static void save_record(const medication_plan_t *plan)
{
  medication_record_t *record;

  if (g_record_count >= 100)
    {
      memmove(&g_records[0], &g_records[1],
              sizeof(medication_record_t) * 99);
      g_record_count = 99;
    }

  record = &g_records[g_record_count];
  memset(record, 0, sizeof(*record));
  strncpy(record->name, plan->name, MEDICATION_NAME_LEN - 1);
  record->scheduled_time = get_time_sec();
  record->taken_time     = 0;
  record->taken          = false;

  g_record_count++;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int medication_restore_defaults(void)
{
  medication_plan_t demo;

  memset(g_plans, 0, sizeof(g_plans));
  memset(g_records, 0, sizeof(g_records));
  g_plan_count   = 0;
  g_record_count = 0;

  memset(&demo, 0, sizeof(demo));
  strncpy(demo.name,  "降压药", MEDICATION_NAME_LEN - 1);
  strncpy(demo.unit,  "片", 7);
  strncpy(demo.notes, "饭后服用", MEDICATION_NOTES_LEN - 1);
  demo.hour    = 8;
  demo.minute  = 0;
  demo.dosage  = 1;
  demo.enabled = true;

  return medication_add_plan(&demo);
}

int medication_save(void)
{
  med_store_t store;

  memset(&store, 0, sizeof(store));
  store.version = STORE_VERSION;
  store.count   = (uint8_t)g_plan_count;
  memcpy(store.plans, g_plans, sizeof(g_plans));

  if (storage_save(STORE_KEY, &store, sizeof(store)) != OK)
    {
      syslog(LOG_WARNING,
             "[%s] 用药计划保存失败（持久化不可用？），改动只在内存里\n",
             LOG_TAG);
      return -EIO;
    }

  syslog(LOG_INFO, "[%s] 已保存 %d 条用药计划\n", LOG_TAG, g_plan_count);
  return OK;
}

int medication_load(void)
{
  med_store_t store;
  int n;

  memset(&store, 0, sizeof(store));

  n = storage_load(STORE_KEY, &store, sizeof(store));
  if (n < 0)
    {
      syslog(LOG_INFO, "[%s] 没有已保存的用药计划 (err=%d)\n", LOG_TAG, n);
      return n;
    }

  if (n != (int)sizeof(store) || store.version != STORE_VERSION ||
      store.count > MAX_MEDICATIONS)
    {
      syslog(LOG_WARNING,
             "[%s] 存档格式不对(读到 %d 字节, 版本 %d, 条数 %d)，忽略\n",
             LOG_TAG, n, store.version, store.count);
      return -EINVAL;
    }

  memcpy(g_plans, store.plans, sizeof(g_plans));
  g_plan_count = store.count;

  syslog(LOG_INFO, "[%s] 已从 /data 恢复 %d 条用药计划\n",
         LOG_TAG, g_plan_count);

  return OK;
}

int medication_init(void)
{
  syslog(LOG_INFO, "[%s] Initializing medication system\n", LOG_TAG);

  memset(g_plans, 0, sizeof(g_plans));
  memset(g_records, 0, sizeof(g_records));
  g_plan_count   = 0;
  g_record_count = 0;

  /* 有存档就用存档，没有就写一条示例方便上手 */

  if (medication_load() != OK)
    {
      medication_restore_defaults();
      syslog(LOG_INFO, "[%s] 使用默认示例计划\n", LOG_TAG);
    }

  g_initialized = true;
  return OK;
}

void medication_deinit(void)
{
  if (g_initialized)
    {
      medication_save();
    }

  g_initialized = false;
  syslog(LOG_INFO, "[%s] Medication system deinitialized\n", LOG_TAG);
}

void medication_process(void)
{
  int minute_of_day;
  uint32_t today;
  uint32_t now;
  int i;

  if (!g_initialized)
    {
      return;
    }

  minute_of_day = get_minute_of_day();
  today         = get_day_stamp();
  now           = get_time_sec();

  for (i = 0; i < g_plan_count; i++)
    {
      medication_plan_t *plan = &g_plans[i];
      int plan_minute = plan->hour * 60 + plan->minute;

      /* 跨天重置：日期戳变了就重新开始今天的记账 */

      if (plan->last_fired_day != today)
        {
          plan->taken            = false;
          plan->remind_count     = 0;
          plan->last_remind_time = 0;
          plan->last_fired_day   = today;

          if (!plan->enabled)
            {
              continue;
            }
        }

      if (!plan->enabled || plan->taken)
        {
          continue;
        }

      /* 还没到点，什么都不做 */

      if (minute_of_day < plan_minute)
        {
          continue;
        }

      /* 到点了：第一次提醒 */

      if (plan->remind_count == 0)
        {
          event_trigger_medication(plan->name);
          save_record(plan);
          plan->remind_count     = 1;
          plan->last_remind_time = now;

          syslog(LOG_INFO, "[%s] 用药提醒: %s (%02d:%02d)\n",
                 LOG_TAG, plan->name, plan->hour, plan->minute);
          continue;
        }

      /* 提醒过但没确认：按间隔重提醒，直到次数用尽 */

      if (plan->remind_count < MAX_REMIND_TIMES &&
          (now - plan->last_remind_time) >= REMIND_INTERVAL_SEC)
        {
          event_trigger_medication(plan->name);
          plan->remind_count++;
          plan->last_remind_time = now;

          syslog(LOG_INFO, "[%s] 用药再次提醒(%d/%d): %s\n",
                 LOG_TAG, plan->remind_count, MAX_REMIND_TIMES, plan->name);
          continue;
        }

      /* 次数用尽且超过判定窗口仍未确认 -> 漏服，通知家属 */

      if (plan->remind_count >= MAX_REMIND_TIMES &&
          (now - plan->last_remind_time) >= MEDICATION_MISSED_SEC)
        {
          syslog(LOG_WARNING, "[%s] 用药漏服: %s\n", LOG_TAG, plan->name);

          cloud_report_alert("medication_missed", plan->name);

          /* 标记为已处理，避免每分钟重复上报 */

          plan->taken = true;
        }
    }
}

int medication_add_plan(const medication_plan_t *plan)
{
  if (plan == NULL)
    {
      return -EINVAL;
    }

  if (g_plan_count >= MAX_MEDICATIONS)
    {
      return -ENOSPC;
    }

  g_plans[g_plan_count] = *plan;
  g_plans[g_plan_count].taken          = false;
  g_plans[g_plan_count].remind_count   = 0;
  g_plans[g_plan_count].last_remind_time = 0;
  g_plans[g_plan_count].last_fired_day   = 0;
  g_plan_count++;

  syslog(LOG_INFO, "[%s] 新增用药计划: %s %02d:%02d\n",
         LOG_TAG, plan->name, plan->hour, plan->minute);

  medication_save();
  return OK;
}

int medication_remove_plan(int index)
{
  int i;

  if (index < 0 || index >= g_plan_count)
    {
      return -EINVAL;
    }

  for (i = index; i < g_plan_count - 1; i++)
    {
      g_plans[i] = g_plans[i + 1];
    }

  g_plan_count--;
  memset(&g_plans[g_plan_count], 0, sizeof(medication_plan_t));

  syslog(LOG_INFO, "[%s] 删除用药计划 index=%d\n", LOG_TAG, index);

  medication_save();
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
  int i;

  if (index < 0 || index >= g_plan_count)
    {
      return -EINVAL;
    }

  g_plans[index].taken          = true;
  g_plans[index].remind_count   = 0;
  g_plans[index].last_remind_time = 0;

  /* 回填最近一条同名且未服用的记录 */

  for (i = g_record_count - 1; i >= 0; i--)
    {
      if (!g_records[i].taken &&
          strncmp(g_records[i].name, g_plans[index].name,
                  MEDICATION_NAME_LEN) == 0)
        {
          g_records[i].taken      = true;
          g_records[i].taken_time = get_time_sec();
          break;
        }
    }

  syslog(LOG_INFO, "[%s] 确认服药: %s\n", LOG_TAG, g_plans[index].name);
  return OK;
}

int medication_snooze(int index)
{
  if (index < 0 || index >= g_plan_count)
    {
      return -EINVAL;
    }

  /* 把上次提醒时间推到"间隔前"，下一次 process 会立刻再响一次，
   * 之后仍受 MAX_REMIND_TIMES 限制 */

  g_plans[index].last_remind_time = get_time_sec() - REMIND_INTERVAL_SEC;

  syslog(LOG_INFO, "[%s] 稍后提醒: %s\n", LOG_TAG, g_plans[index].name);
  return OK;
}

bool medication_has_pending(void)
{
  int i;

  for (i = 0; i < g_plan_count; i++)
    {
      if (g_plans[i].enabled && !g_plans[i].taken)
        {
          return true;
        }
    }

  return false;
}

int medication_get_records(medication_record_t *records, int max_count)
{
  int count;

  if (records == NULL || max_count <= 0)
    {
      return 0;
    }

  count = (g_record_count < max_count) ? g_record_count : max_count;
  memcpy(records, g_records, count * sizeof(medication_record_t));

  return count;
}

/*--------------------------------------------------------------------------
 * 编辑接口
 *------------------------------------------------------------------------*/

int medication_set_time(int index, uint8_t hour, uint8_t minute)
{
  if (index < 0 || index >= g_plan_count)
    {
      return -EINVAL;
    }

  if (hour > 23 || minute > 59)
    {
      return -EINVAL;
    }

  g_plans[index].hour   = hour;
  g_plans[index].minute = minute;

  /* 改了时间就当今天还没处理过，重新记账 */

  g_plans[index].taken          = false;
  g_plans[index].remind_count   = 0;
  g_plans[index].last_fired_day = 0;

  medication_save();
  return OK;
}

int medication_set_dosage(int index, uint8_t dosage)
{
  if (index < 0 || index >= g_plan_count)
    {
      return -EINVAL;
    }

  g_plans[index].dosage = dosage;
  medication_save();
  return OK;
}

int medication_set_enabled(int index, bool enabled)
{
  if (index < 0 || index >= g_plan_count)
    {
      return -EINVAL;
    }

  g_plans[index].enabled = enabled;
  medication_save();
  return OK;
}

int medication_set_name(int index, const char *name)
{
  if (index < 0 || index >= g_plan_count || name == NULL)
    {
      return -EINVAL;
    }

  strncpy(g_plans[index].name, name, MEDICATION_NAME_LEN - 1);
  g_plans[index].name[MEDICATION_NAME_LEN - 1] = '\0';

  medication_save();
  return OK;
}

/*--------------------------------------------------------------------------
 * 预设
 *------------------------------------------------------------------------*/

int medication_preset_count(void)
{
  return PRESET_COUNT;
}

const char *medication_preset_name(int index)
{
  if (index < 0 || index >= PRESET_COUNT)
    {
      return NULL;
    }

  return g_presets[index];
}

int medication_add_preset(int preset_index, uint8_t hour, uint8_t minute,
                          uint8_t dosage)
{
  medication_plan_t plan;

  if (preset_index < 0)
    {
      preset_index = 0;
    }

  preset_index %= PRESET_COUNT;

  memset(&plan, 0, sizeof(plan));
  strncpy(plan.name, g_presets[preset_index], MEDICATION_NAME_LEN - 1);
  strncpy(plan.unit, "片", 7);
  plan.hour    = hour;
  plan.minute  = minute;
  plan.dosage  = (dosage == 0) ? 1 : dosage;
  plan.enabled = true;

  return medication_add_plan(&plan);
}

int medication_sync_from_cloud(void)
{
  /* 云端通信整层还是桩（见 cloud.c），这里如实说明，不再假装成功 */

  syslog(LOG_INFO,
         "[%s] 云端同步未启用（cloud 模块当前为本地桩）\n", LOG_TAG);
  return -ENOSYS;
}
