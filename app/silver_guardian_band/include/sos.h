/****************************************************************************
 * Silver Guardian Band - SOS 呼救模块头文件
 ****************************************************************************/

#ifndef __APP_SILVER_GUARDIAN_BAND_INCLUDE_SOS_H
#define __APP_SILVER_GUARDIAN_BAND_INCLUDE_SOS_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdbool.h>
#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* SOS 按键配置 */

#define SOS_BUTTON_PIN          1   /* GPIO PA01 */
#define SOS_PRESS_DURATION_MS   3000 /* 长按 3 秒触发 */
#define SOS_CANCEL_TIMEOUT_MS   10000 /* 10 秒内可取消 */

/* SOS 状态定义 */

#define SOS_STATE_IDLE          0   /* 空闲状态 */
#define SOS_STATE_PRESSING      1   /* 按下计时中 */
#define SOS_STATE_TRIGGERED     2   /* 已触发 */
#define SOS_STATE_CANCELLING    3   /* 可取消状态 */

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* SOS 事件类型 */

typedef enum
{
  SOS_EVENT_NONE = 0,       /* 无事件 */
  SOS_EVENT_TRIGGERED,      /* SOS 已触发 */
  SOS_EVENT_CANCELLED,      /* SOS 已取消 */
  SOS_EVENT_TIMEOUT,        /* 取消超时 */
  SOS_EVENT_SENT            /* 已发送到 Hub */
} sos_event_t;

/* SOS 状态结构体 */

typedef struct
{
  uint8_t state;            /* 当前状态 */
  uint32_t press_start;     /* 按下开始时间 */
  uint32_t trigger_time;    /* 触发时间 */
  bool is_triggered;        /* 是否已触发 */
  bool is_sent;             /* 是否已发送 */
} sos_status_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * @brief 初始化 SOS 模块
 * @return 0 成功，负数失败
 */

int sos_init(void);

/**
 * @brief 反初始化 SOS 模块
 */

void sos_deinit(void);

/**
 * @brief 检查 SOS 状态（主循环调用）
 */

void sos_check(void);

/**
 * @brief 获取 SOS 状态
 * @return SOS 状态结构体指针
 */

const sos_status_t *sos_get_status(void);

/**
 * @brief 手动触发 SOS（用于测试）
 * @return 0 成功
 */

int sos_trigger_manual(void);

/**
 * @brief 取消 SOS
 * @return 0 成功，负数失败（已超过取消时间）
 */

int sos_cancel(void);

/**
 * @brief 注册 SOS 事件回调
 * @param callback 回调函数
 */

typedef void (*sos_event_callback_t)(sos_event_t event);
void sos_register_callback(sos_event_callback_t callback);

/**
 * @brief 检查 SOS 是否已触发
 * @return true 已触发，false 未触发
 */

bool sos_is_triggered(void);

/**
 * @brief 重置 SOS 状态
 */

void sos_reset(void);

#endif /* __APP_SILVER_GUARDIAN_BAND_INCLUDE_SOS_H */
