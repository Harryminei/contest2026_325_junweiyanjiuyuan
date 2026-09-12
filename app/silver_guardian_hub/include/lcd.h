/****************************************************************************
 * Silver Guardian Hub - 显示层头文件
 *
 * 本文件负责的是"底座"：LVGL 初始化、显示与输入的绑定、顶部状态栏、
 * 以及盖在所有页面之上的警示层（SOS / 久坐 / 用药）。
 * 具体页面在 ui.c 里，通过 ui_init() 挂到内容区上。
 *
 * 硬件：润芯微 Gemini-S1 + 2.8 寸 ILI9341 SPI 屏（320x240）+ GT911 电容触摸
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

/* 警示类型（决定警示层配色与图标） */

typedef enum
{
  LCD_ALERT_NONE = 0,
  LCD_ALERT_SOS,          /* 红：紧急求助 */
  LCD_ALERT_SITTING,      /* 橙：久坐提醒 */
  LCD_ALERT_MEDICATION    /* 蓝：用药提醒 */
} lcd_alert_t;

/* 顶部返回按钮回调 */

typedef void (*lcd_back_cb_t)(void);

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * @brief 初始化显示层（LVGL + /dev/lcd0 + /dev/input0 + 触摸探针 + 页面）
 * @return 0 成功；负值失败
 */

int silver_lcd_init(void);

/**
 * @brief 反初始化显示层
 */

void lcd_deinit(void);

/**
 * @brief 主循环调用：驱动 LVGL、轮询触摸探针、处理演示模式
 */

void lcd_task(void);

/*--------------------------------------------------------------------------
 * 顶部状态栏
 *------------------------------------------------------------------------*/

/**
 * @brief 设置状态栏标题
 */

void lcd_set_title(const char *text);

/**
 * @brief 显示/隐藏状态栏左侧的"返回"按钮
 */

void lcd_set_back_visible(bool visible);

/**
 * @brief 注册"返回"按钮回调
 */

void lcd_set_back_callback(lcd_back_cb_t cb);

/**
 * @brief 设置网络状态显示
 */

void lcd_set_network(bool connected, int8_t signal);

/**
 * @brief 设置底部守护状态文字
 */

void lcd_set_guard_status(const char *text);

/*--------------------------------------------------------------------------
 * 警示层
 *------------------------------------------------------------------------*/

/**
 * @brief 弹出警示层
 * @param kind  警示类型（决定配色）
 * @param title 标题
 * @param body  正文（可含 \n）
 */

void lcd_show_alert(lcd_alert_t kind, const char *title, const char *body);

/**
 * @brief 收起警示层
 */

void lcd_clear_alert(void);

/**
 * @brief 警示层当前是否可见
 */

bool lcd_is_alert_active(void);

/*--------------------------------------------------------------------------
 * 触摸与演示模式
 *------------------------------------------------------------------------*/

/**
 * @brief LVGL 的触摸输入设备是否创建成功
 */

bool lcd_touch_ready(void);

/**
 * @brief 演示模式：无触摸时自动轮播页面（也用于给评委演示）
 */

void lcd_set_demo_mode(bool on);
bool lcd_get_demo_mode(void);

/**
 * @brief 当前毫秒计时（LVGL tick）
 */

uint32_t lcd_tick_ms(void);

#endif /* __APP_SILVER_GUARDIAN_HUB_INCLUDE_LCD_H */
