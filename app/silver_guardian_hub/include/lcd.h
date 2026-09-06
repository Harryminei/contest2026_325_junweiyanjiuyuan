/****************************************************************************
 * Silver Guardian Hub - LCD 界面模块头文件
 *
 * 基于 LVGL 的界面层：主界面(时间/状态) + 事件警示界面(SOS/久坐/用药)
 * 硬件平台：润芯微 Gemini-S1（2.8寸 ILI9341 SPI 屏, 320x240）
 ****************************************************************************/

#ifndef __APP_SILVER_GUARDIAN_HUB_INCLUDE_LCD_H
#define __APP_SILVER_GUARDIAN_HUB_INCLUDE_LCD_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdbool.h>
#include <stdint.h>

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* 界面视图类型 */

typedef enum
{
  LCD_VIEW_MAIN = 0,      /* 主界面 */
  LCD_VIEW_SOS,           /* SOS 紧急呼救 */
  LCD_VIEW_SITTING,       /* 久坐提醒 */
  LCD_VIEW_MEDICATION     /* 用药提醒 */
} lcd_view_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * @brief 初始化 LCD 界面（LVGL + 显示绑定 + 界面创建）
 * @return 0 成功；负值失败
 */

int silver_lcd_init(void);

/**
 * @brief 反初始化 LCD 界面
 */

void lcd_deinit(void);

/**
 * @brief 主循环调用：驱动 LVGL 定时器并刷新时钟
 */

void lcd_task(void);

/**
 * @brief 刷新状态栏（时钟 / WiFi 状态 / 守护状态）
 */

void lcd_update_status(void);

/**
 * @brief 显示 SOS 紧急呼救界面
 */

void lcd_show_sos(void);

/**
 * @brief 显示久坐提醒界面
 * @param minutes 已久坐分钟数
 */

void lcd_show_sitting(uint32_t minutes);

/**
 * @brief 显示用药提醒界面
 * @param name 药品名称
 * @param dosage 剂量
 * @param unit 单位
 */

void lcd_show_medication(const char *name, uint8_t dosage,
                         const char *unit);

/**
 * @brief 退出警示界面，返回主界面
 */

void lcd_clear_alert(void);

/**
 * @brief 设置状态栏文字（如"守护中"）
 * @param text 状态文字
 */

void lcd_set_status(const char *text);

/**
 * @brief 设置 WiFi / 云端连接状态
 * @param connected 是否连接
 * @param signal 信号强度(dBm)
 */

void lcd_set_network(bool connected, int8_t signal);

/**
 * @brief 是否有警示界面正在显示
 * @return true 是
 */

bool lcd_is_alert_active(void);

/**
 * @brief 获取当前界面视图类型
 * @return 视图类型
 */

lcd_view_t lcd_get_view(void);

#endif /* __APP_SILVER_GUARDIAN_HUB_INCLUDE_LCD_H */
