/****************************************************************************
 * Silver Guardian Hub - 用药提醒模块头文件
 *
 * 计划保存在内存里，同时通过 storage 模块落盘到 /data，重启不丢。
 ****************************************************************************/

#ifndef __APP_SILVER_GUARDIAN_HUB_INCLUDE_MEDICATION_H
#define __APP_SILVER_GUARDIAN_HUB_INCLUDE_MEDICATION_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdbool.h>
#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define MAX_MEDICATIONS         10
#define MEDICATION_NAME_LEN     32
#define MEDICATION_NOTES_LEN    64
#define MAX_REMIND_TIMES        3
#define REMIND_INTERVAL_SEC     (5 * 60)   /* 两次提醒间隔 */
#define MEDICATION_MISSED_SEC   (30 * 60)  /* 提醒用尽后再等多久判定为漏服 */

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* 用药计划 */

typedef struct
{
  char     name[MEDICATION_NAME_LEN];  /* 药品名称 */
  uint8_t  hour;                       /* 提醒小时 */
  uint8_t  minute;                     /* 提醒分钟 */
  uint8_t  dosage;                     /* 剂量 */
  char     unit[8];                    /* 单位（片、粒、ml…） */
  char     notes[MEDICATION_NOTES_LEN];/* 备注 */
  bool     enabled;                    /* 是否启用 */
  bool     taken;                      /* 今天是否已服 */
  uint8_t  remind_count;               /* 今天已提醒次数 */
  uint32_t last_remind_time;           /* 上次提醒时间（秒） */
  uint32_t last_fired_day;             /* 上次触发那天的日期戳，用来做跨天判定 */
} medication_plan_t;

/* 服药记录 */

typedef struct
{
  char     name[MEDICATION_NAME_LEN];
  uint32_t scheduled_time;             /* 计划时间 */
  uint32_t taken_time;                 /* 实际服药时间 */
  bool     taken;                      /* 是否服用 */
} medication_record_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * @brief 初始化用药提醒（会尝试从 /data 读取已保存的计划）
 * @return 0 成功
 */

int medication_init(void);

/**
 * @brief 反初始化
 */

void medication_deinit(void);

/**
 * @brief 主循环调用：到点触发提醒、判定漏服、跨天重置
 */

void medication_process(void);

/**
 * @brief 添加用药计划
 * @return 0 成功；-ENOSPC 已满
 */

int medication_add_plan(const medication_plan_t *plan);

/**
 * @brief 删除用药计划
 */

int medication_remove_plan(int index);

/**
 * @brief 取用药计划
 * @return 计划指针，NULL 表示索引无效
 */

const medication_plan_t *medication_get_plan(int index);

/**
 * @brief 计划数量
 */

int medication_get_plan_count(void);

/**
 * @brief 确认服药
 */

int medication_confirm_taken(int index);

/**
 * @brief 稍后提醒（间隔 REMIND_INTERVAL_SEC 后再响一次）
 */

int medication_snooze(int index);

/**
 * @brief 是否有待处理（已启用且今天未服）的计划
 */

bool medication_has_pending(void);

/**
 * @brief 取服药记录
 * @return 实际条数
 */

int medication_get_records(medication_record_t *records, int max_count);

/*--------------------------------------------------------------------------
 * 编辑接口（触摸 UI 用）
 *------------------------------------------------------------------------*/

/**
 * @brief 修改提醒时间
 */

int medication_set_time(int index, uint8_t hour, uint8_t minute);

/**
 * @brief 修改剂量
 */

int medication_set_dosage(int index, uint8_t dosage);

/**
 * @brief 启用/停用
 */

int medication_set_enabled(int index, bool enabled);

/**
 * @brief 修改药品名
 */

int medication_set_name(int index, const char *name);

/*--------------------------------------------------------------------------
 * 预设药品（触屏上没法打中文，新增计划时从预设里挑）
 *------------------------------------------------------------------------*/

/**
 * @brief 预设药品数量
 */

int medication_preset_count(void);

/**
 * @brief 取第 index 个预设药品名
 */

const char *medication_preset_name(int index);

/**
 * @brief 用预设快速新增一个计划
 * @param preset_index 预设索引（越界会取模）
 * @return 0 成功
 */

int medication_add_preset(int preset_index, uint8_t hour, uint8_t minute,
                          uint8_t dosage);

/*--------------------------------------------------------------------------
 * 持久化
 *------------------------------------------------------------------------*/

/**
 * @brief 把当前计划写入 /data（失败只记日志）
 */

int medication_save(void);

/**
 * @brief 从 /data 读取计划；没有存档时保持当前内存内容
 * @return 0 成功读取；-ENOENT 没有存档；其他负值失败
 */

int medication_load(void);

/**
 * @brief 恢复出厂默认计划（清空并写入一条示例）
 */

int medication_restore_defaults(void);

#endif /* __APP_SILVER_GUARDIAN_HUB_INCLUDE_MEDICATION_H */
