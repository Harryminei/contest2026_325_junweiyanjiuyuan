/****************************************************************************
 * Silver Guardian Band - UI 界面模块头文件
 ****************************************************************************/

#ifndef __APP_SILVER_GUARDIAN_BAND_INCLUDE_UI_H
#define __APP_SILVER_GUARDIAN_BAND_INCLUDE_UI_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdbool.h>
#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* 屏幕尺寸 */

#define SCREEN_WIDTH            240
#define SCREEN_HEIGHT           240

/* 字体大小 */

#define FONT_SIZE_LARGE         32
#define FONT_SIZE_MEDIUM        24
#define FONT_SIZE_SMALL         16

/* UI 事件类型 */

#define UI_EVENT_NONE           0
#define UI_EVENT_BUTTON_PRESS   1
#define UI_EVENT_BUTTON_LONG    2
#define UI_EVENT_TOUCH          3

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* UI 界面类型 */

typedef enum
{
  UI_SCREEN_MAIN = 0,        /* 主界面 */
  UI_SCREEN_SOS,             /* SOS 界面 */
  UI_SCREEN_SOS_CANCEL,      /* SOS 取消界面 */
  UI_SCREEN_SITTING,         /* 久坐提醒界面 */
  UI_SCREEN_ALERT,           /* 告警界面 */
  UI_SCREEN_MENU             /* 菜单界面 */
} ui_screen_t;

/* UI 事件结构体 */

typedef struct
{
  uint8_t type;              /* 事件类型 */
  uint16_t x;                /* X 坐标 */
  uint16_t y;                /* Y 坐标 */
  uint32_t param;            /* 参数 */
} ui_event_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * @brief 初始化 UI 模块
 * @return 0 成功，负数失败
 */

int ui_init(void);

/**
 * @brief 反初始化 UI 模块
 */

void ui_deinit(void);

/**
 * @brief 处理 UI 事件（主循环调用）
 */

void ui_process_events(void);

/**
 * @brief 显示主界面
 */

void ui_show_main_screen(void);

/**
 * @brief 显示 SOS 界面
 */

void ui_show_sos_screen(void);

/**
 * @brief 显示 SOS 取消提示
 */

void ui_show_sos_cancel_prompt(void);

/**
 * @brief 显示久坐提醒
 * @param duration 久坐时间（分钟）
 */

void ui_show_sitting_reminder(uint32_t duration);

/**
 * @brief 显示异常告警
 * @param message 告警消息
 */

void ui_show_alert_screen(const char *message);

/**
 * @brief 更新时间显示
 */

void ui_update_time(void);

/**
 * @brief 更新步数显示
 * @param steps 步数
 */

void ui_update_steps(uint32_t steps);

/**
 * @brief 更新电池显示
 * @param level 电量百分比
 */

void ui_update_battery(uint8_t level);

/**
 * @brief 更新 BLE 状态显示
 * @param connected 是否连接
 */

void ui_update_ble_status(bool connected);

/**
 * @brief 显示消息
 * @param message 消息内容
 * @param duration 显示时长（秒）
 */

void ui_show_message(const char *message, uint32_t duration);

/**
 * @brief 清除屏幕
 */

void ui_clear_screen(void);

/**
 * @brief 刷新显示
 */

void ui_refresh(void);

/**
 * @brief 震动马达控制（短震动）
 */

void motor_vibrate_short(void);

/**
 * @brief 震动马达控制（长震动）
 */

void motor_vibrate_long(void);

/**
 * @brief 震动马达控制（SOS 模式）
 */

void motor_vibrate_sos(void);

/**
 * @brief 震动马达控制（停止）
 */

void motor_stop(void);

#endif /* __APP_SILVER_GUARDIAN_BAND_INCLUDE_UI_H */
