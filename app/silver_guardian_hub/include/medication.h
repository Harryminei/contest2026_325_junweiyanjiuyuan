/****************************************************************************
 * Silver Guardian Hub - 用药提醒模块头文件
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
#define REMIND_INTERVAL_SEC     (5 * 60)  /* 5分钟 */

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* 用药计划 */

typedef struct
{
  char name[MEDICATION_NAME_LEN];     /* 药品名称 */
  uint8_t hour;                        /* 提醒小时 */
  uint8_t minute;                      /* 提醒分钟 */
  uint8_t dosage;                      /* 剂量 */
  char unit[8];                        /* 单位（片、ml等） */
  char notes[MEDICATION_NOTES_LEN];   /* 备注 */
  bool enabled;                        /* 是否启用 */
  bool taken;                          /* 是否已服用 */
  uint8_t remind_count;               /* 已提醒次数 */
  uint32_t last_remind_time;          /* 上次提醒时间 */
} medication_plan_t;

/* 服药记录 */

typedef struct
{
  char name[MEDICATION_NAME_LEN];
  uint32_t scheduled_time;            /* 计划时间 */
  uint32_t taken_time;                /* 实际服药时间 */
  bool taken;                         /* 是否服用 */
} medication_record_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * @brief 初始化用药提醒系统
 * @return 0 成功
 */

int medication_init(void);

/**
 * @brief 反初始化用药提醒系统
 */

void medication_deinit(void);

/**
 * @brief 处理用药提醒（主循环调用）
 */

void medication_process(void);

/**
 * @brief 添加用药计划
 * @param plan 用药计划
 * @return 0 成功
 */

int medication_add_plan(const medication_plan_t *plan);

/**
 * @brief 删除用药计划
 * @param index 计划索引
 * @return 0 成功
 */

int medication_remove_plan(int index);

/**
 * @brief 获取用药计划
 * @param index 计划索引
 * @return 计划指针，NULL 表示无效
 */

const medication_plan_t *medication_get_plan(int index);

/**
 * @brief 获取计划数量
 * @return 计划数量
 */

int medication_get_plan_count(void);

/**
 * @brief 确认服药
 * @param index 计划索引
 * @return 0 成功
 */

int medication_confirm_taken(int index);

/**
 * @brief 稍后提醒
 * @param index 计划索引
 * @return 0 成功
 */

int medication_snooze(int index);

/**
 * @brief 检查是否有待提醒的用药
 * @return true 有待提醒
 */

bool medication_has_pending(void);

/**
 * @brief 获取服药记录
 * @param records 记录数组
 * @param max_count 最大记录数
 * @return 实际记录数
 */

int medication_get_records(medication_record_t *records,
                           int max_count);

/**
 * @brief 从云端同步用药计划
 * @return 0 成功
 */

int medication_sync_from_cloud(void);

#endif /* __APP_SILVER_GUARDIAN_HUB_INCLUDE_MEDICATION_H */
