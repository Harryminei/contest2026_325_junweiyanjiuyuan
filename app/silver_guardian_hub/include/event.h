/****************************************************************************
 * Silver Guardian Hub - 事件处理模块头文件
 ****************************************************************************/

#ifndef __APP_SILVER_GUARDIAN_HUB_INCLUDE_EVENT_H
#define __APP_SILVER_GUARDIAN_HUB_INCLUDE_EVENT_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdbool.h>
#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* 事件类型 */

#define EVENT_TYPE_NONE             0
#define EVENT_TYPE_SOS              1   /* SOS 告警 */
#define EVENT_TYPE_SOS_CANCEL       2   /* SOS 取消 */
#define EVENT_TYPE_SITTING          3   /* 久坐提醒 */
#define EVENT_TYPE_STILL_ALERT      4   /* 静止告警 */
#define EVENT_TYPE_ACTIVITY_RESUMED 5   /* 恢复活动 */
#define EVENT_TYPE_MEDICATION       6   /* 用药提醒 */
#define EVENT_TYPE_CLOUD_CMD        7   /* 云端命令 */

/* 事件优先级 */

#define EVENT_PRIORITY_LOW          0
#define EVENT_PRIORITY_MEDIUM       1
#define EVENT_PRIORITY_HIGH         2
#define EVENT_PRIORITY_CRITICAL     3

/* 事件历史条数（事件记录页显示用） */

#define EVENT_HISTORY_SIZE          32

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* 事件结构体 */

typedef struct
{
  uint8_t type;              /* 事件类型 */
  uint8_t priority;          /* 优先级 */
  uint32_t timestamp;        /* 时间戳 */
  uint32_t param;            /* 参数 */
  char message[128];         /* 消息内容 */
} event_t;

/* 事件回调函数类型 */

typedef void (*event_handler_t)(const event_t *event);

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * @brief 初始化事件系统
 * @return 0 成功
 */

int event_init(void);

/**
 * @brief 反初始化事件系统
 */

void event_deinit(void);

/**
 * @brief 处理事件（主循环调用）
 */

void event_process(void);

/**
 * @brief 发送事件
 * @param event 事件结构体
 * @return 0 成功
 */

int event_send(const event_t *event);

/**
 * @brief 注册事件处理器
 * @param type 事件类型
 * @param handler 处理函数
 */

void event_register_handler(uint8_t type, event_handler_t handler);

/**
 * @brief 触发 SOS 事件
 */

void event_trigger_sos(void);

/**
 * @brief 触发 SOS 取消事件
 */

void event_trigger_sos_cancel(void);

/**
 * @brief 触发久坐提醒事件
 * @param duration 久坐时间（秒）
 */

void event_trigger_sitting(uint32_t duration);

/**
 * @brief 触发恢复活动事件
 */

void event_trigger_activity_resumed(void);

/**
 * @brief 触发用药提醒事件
 * @param drug_name 药品名称
 */

void event_trigger_medication(const char *drug_name);

/**
 * @brief 获取事件队列中的事件数量
 * @return 事件数量
 */

int event_get_count(void);

/**
 * @brief 清空事件队列
 */

void event_clear_queue(void);

/**
 * @brief 取事件历史（已分发过的事件，最新的在前）
 * @param out       输出数组
 * @param max_count 数组容量
 * @return 实际写入条数
 */

int event_get_history(event_t *out, int max_count);

/**
 * @brief 清空事件历史
 */

void event_clear_history(void);

/**
 * @brief 事件类型对应的中文名（事件记录页显示用）
 */

const char *event_type_name(uint8_t type);

#endif /* __APP_SILVER_GUARDIAN_HUB_INCLUDE_EVENT_H */
