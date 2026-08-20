/****************************************************************************
 * Silver Guardian Band - BLE 通信模块头文件
 ****************************************************************************/

#ifndef __APP_SILVER_GUARDIAN_BAND_INCLUDE_BLE_H
#define __APP_SILVER_GUARDIAN_BAND_INCLUDE_BLE_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdbool.h>
#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* BLE GATT Service UUID */

#define BLE_SERVICE_UUID            0x1820

/* BLE Characteristic UUID */

#define BLE_CHAR_SOS_ALERT          0x2A01
#define BLE_CHAR_ACTIVITY_STATUS    0x2A02
#define BLE_CHAR_HEALTH_DATA        0x2A03
#define BLE_CHAR_SYNC_COMMAND       0x2A04
#define BLE_CHAR_BATTERY_LEVEL      0x2A19

/* BLE 连接状态 */

#define BLE_STATE_DISCONNECTED      0
#define BLE_STATE_CONNECTING        1
#define BLE_STATE_CONNECTED         2
#define BLE_STATE_ADVERTISING       3

/* BLE 告警类型 */

#define BLE_ALERT_NONE              0x00
#define BLE_ALERT_SOS               0x01
#define BLE_ALERT_FALL              0x02
#define BLE_ALERT_ABNORMAL          0x03

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* BLE 状态结构体 */

typedef struct
{
  uint8_t state;               /* 连接状态 */
  bool is_advertising;         /* 是否在广播 */
  bool is_connected;           /* 是否已连接 */
  uint16_t conn_handle;        /* 连接句柄 */
  int8_t rssi;                 /* 信号强度 */
  uint32_t last_sync_time;     /* 最后同步时间 */
} ble_status_t;

/* SOS 告警数据 */

typedef struct __attribute__((packed))
{
  uint8_t alert_type;          /* 告警类型 */
  uint32_t timestamp;          /* 时间戳 */
  uint8_t battery_level;       /* 电池电量 */
} ble_sos_alert_t;

/* 活动状态数据 */

typedef struct __attribute__((packed))
{
  uint8_t activity_type;       /* 活动类型 */
  uint16_t duration;           /* 持续时间（秒） */
  uint16_t steps;              /* 步数 */
  uint32_t timestamp;          /* 时间戳 */
} ble_activity_status_t;

/* 健康数据 */

typedef struct __attribute__((packed))
{
  uint16_t heart_rate;         /* 心率 */
  uint8_t spo2;                /* 血氧 */
  uint16_t systolic;           /* 收缩压 */
  uint16_t diastolic;          /* 舒张压 */
  uint16_t steps;              /* 步数 */
  uint32_t timestamp;          /* 时间戳 */
} ble_health_data_t;

/* BLE 事件类型 */

typedef enum
{
  BLE_EVENT_CONNECTED = 0,
  BLE_EVENT_DISCONNECTED,
  BLE_EVENT_DATA_SENT,
  BLE_EVENT_SYNC_RECEIVED
} ble_event_t;

/* BLE 事件回调 */

typedef void (*ble_event_callback_t)(ble_event_t event);

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * @brief 初始化 BLE 模块
 * @return 0 成功，负数失败
 */

int ble_init(void);

/**
 * @brief 反初始化 BLE 模块
 */

void ble_deinit(void);

/**
 * @brief BLE 处理（主循环调用）
 */

void ble_process(void);

/**
 * @brief 开始 BLE 广播
 * @return 0 成功
 */

int ble_start_advertising(void);

/**
 * @brief 停止 BLE 广播
 * @return 0 成功
 */

int ble_stop_advertising(void);

/**
 * @brief 发送 SOS 告警
 * @return 0 成功，负数失败
 */

int ble_send_sos_alert(void);

/**
 * @brief 发送 SOS 取消
 * @return 0 成功
 */

int ble_send_sos_cancel(void);

/**
 * @brief 发送活动状态
 * @param status 活动状态数据
 * @return 0 成功
 */

int ble_send_activity_status(const ble_activity_status_t *status);

/**
 * @brief 发送健康数据
 * @param data 健康数据
 * @return 0 成功
 */

int ble_send_health_data(const ble_health_data_t *data);

/**
 * @brief 获取 BLE 状态
 * @return BLE 状态结构体指针
 */

const ble_status_t *ble_get_status(void);

/**
 * @brief 检查是否已连接
 * @return true 已连接，false 未连接
 */

bool ble_is_connected(void);

/**
 * @brief 获取电池电量
 * @return 电量百分比（0-100）
 */

uint8_t ble_get_battery_level(void);

/**
 * @brief 注册事件回调
 * @param callback 回调函数
 */

void ble_register_callback(ble_event_callback_t callback);

/**
 * @brief 更新电池电量
 * @param level 电量百分比
 */

void ble_update_battery_level(uint8_t level);

/**
 * @brief 同步时间
 * @param timestamp 时间戳
 * @return 0 成功
 */

int ble_sync_time(uint32_t timestamp);

/**
 * @brief 获取 RSSI
 * @return 信号强度
 */

int8_t ble_get_rssi(void);

#endif /* __APP_SILVER_GUARDIAN_BAND_INCLUDE_BLE_H */
