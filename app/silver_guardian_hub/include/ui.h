/****************************************************************************
 * Silver Guardian Hub - 页面层头文件
 *
 * 页面结构（320x240 屏）：顶部 32px 状态栏由 lcd.c 维护，下面是 208px 内容区，
 * 所有页面都是内容区的子对象，同一时刻只显示一个。
 ****************************************************************************/

#ifndef __APP_SILVER_GUARDIAN_HUB_INCLUDE_UI_H
#define __APP_SILVER_GUARDIAN_HUB_INCLUDE_UI_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdbool.h>
#include <stdint.h>

#include <lvgl/lvgl.h>

/****************************************************************************
 * Public Types
 ****************************************************************************/

typedef enum
{
  UI_PAGE_HOME = 0,       /* 主页：时钟 / 日期 / 守护状态 */
  UI_PAGE_MENU,           /* 功能菜单 */
  UI_PAGE_MEDICATION,     /* 用药提醒 */
  UI_PAGE_MED_EDIT,       /* 用药计划编辑 */
  UI_PAGE_HEALTH,         /* 健康数据（板上传感器） */
  UI_PAGE_EVENTS,         /* 事件记录 */
  UI_PAGE_SETTINGS,       /* 设置 */
  UI_PAGE_ABOUT,          /* 关于 / 自检 */
  UI_PAGE_TOUCHTEST,      /* 触摸自检 */
  UI_PAGE_COUNT
} ui_page_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * @brief 创建所有页面
 * @param content_parent 内容区容器（由 lcd.c 提供）
 * @return 0 成功
 */

int ui_init(lv_obj_t *content_parent);

/**
 * @brief 主循环调用：刷新当前页面的动态内容
 * @param now_ms 当前毫秒
 */

void ui_tick(uint32_t now_ms);

/**
 * @brief 切换到指定页面（会压栈，可返回）
 */

void ui_navigate(ui_page_t page);

/**
 * @brief 返回上一页
 */

void ui_back(void);

/**
 * @brief 回到主页并清空返回栈
 */

void ui_go_home(void);

/**
 * @brief 当前页面
 */

ui_page_t ui_current_page(void);

/**
 * @brief 演示模式：自动轮播页面
 */

void ui_set_demo_mode(bool on);
bool ui_get_demo_mode(void);

/**
 * @brief 通知"用户有输入了"，用于退出演示模式
 */

void ui_notify_user_input(void);

#endif /* __APP_SILVER_GUARDIAN_HUB_INCLUDE_UI_H */
