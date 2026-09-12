/****************************************************************************
 * Silver Guardian Hub - 页面层实现
 *
 * 9 个页面挂在 lcd.c 提供的内容区（320x176）上，同一时刻只显示一个。
 * 导航用一个小栈：进入子页面时压栈，顶部"返回"按钮出栈。
 *
 * 单板可交互的全部功能都在这里：
 *   主页      时钟/日期 + 两个入口
 *   功能菜单  六个功能入口
 *   用药提醒  计划列表（点选编辑）+ 新增 + 立即测试提醒
 *   编辑用药  时间/剂量调整、启用停用、删除
 *   健康数据  板上 SHTC3/LTR553/SGP30 真实读数
 *   事件记录  最近发生的事件
 *   设置      提示音量 / 久坐阈值 / 时间校准
 *   关于      版本、运行时长、内存、模块自检
 *   触摸自检  原始触摸事件、坐标范围、LVGL 输入状态
 *
 * 每个按钮的点击处理里都会调 ui_notify_user_input()，用来退出演示模式。
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <sys/time.h>
#include <malloc.h>

#include <lvgl/lvgl.h>

#include "include/ui.h"
#include "include/lcd.h"
#include "include/fonts.h"
#include "include/event.h"
#include "include/medication.h"
#include "include/sensors.h"
#include "include/audio.h"
#include "include/cloud.h"
#include "include/storage.h"
#include "include/touch.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LOG_TAG "ui"

#define PAGE_W      320
#define PAGE_H      176

#define COLOR_BG        0x000000
#define COLOR_CARD      0x1A2430
#define COLOR_CARD_HI   0x2A3A4A
#define COLOR_TEXT      0xFFFFFF
#define COLOR_DIM       0xA8A8A8
#define COLOR_ACCENT    0x2E9BFF
#define COLOR_OK        0x35C46A
#define COLOR_WARN      0xE0A000
#define COLOR_DANGER    0xD03030

#define APP_VERSION     "1.1.0"

/* 设置项的默认值 */

#define DEFAULT_SIT_MINUTES  60
#define DEFAULT_VOLUME       70

/****************************************************************************
 * Private Types
 ****************************************************************************/

typedef struct
{
  uint8_t volume;         /* 提示音量 0-100 */
  uint16_t sit_minutes;   /* 久坐提醒阈值（分钟） */
} sg_settings_t;

/****************************************************************************
 * Private Data
 ****************************************************************************/

static lv_obj_t  *g_page[UI_PAGE_COUNT];
static ui_page_t   g_current = UI_PAGE_HOME;
static ui_page_t   g_stack[8];
static int         g_stack_depth;
static bool        g_demo;

static sg_settings_t g_settings =
{
  DEFAULT_VOLUME,
  DEFAULT_SIT_MINUTES
};

/* 用药编辑页当前选中的计划 */

static int g_edit_index;

/* 主页 */

static lv_obj_t *g_home_clock;
static lv_obj_t *g_home_date;

/* 用药列表 */

static lv_obj_t *g_med_list;
static lv_obj_t *g_med_hint;

/* 用药编辑页 */

static lv_obj_t *g_edit_title;
static lv_obj_t *g_edit_time;
static lv_obj_t *g_edit_dosage;
static lv_obj_t *g_edit_state;

/* 健康页数值标签 */

static lv_obj_t *g_health_value[SG_SENSOR_COUNT];
static lv_obj_t *g_health_state[SG_SENSOR_COUNT];

/* 事件页 */

static lv_obj_t *g_event_list;

/* 设置页 */

static lv_obj_t *g_set_volume;
static lv_obj_t *g_set_sit;
static lv_obj_t *g_set_clock_h;
static lv_obj_t *g_set_clock_m;

/* 关于页 */

static lv_obj_t *g_about_runtime;
static lv_obj_t *g_about_mem;
static lv_obj_t *g_about_modules;
static lv_obj_t *g_about_storage;

/* 触摸自检页 */

static lv_obj_t *g_touch_stats;
static lv_obj_t *g_touch_lvgl;
static lv_obj_t *g_touch_marker;
static lv_obj_t *g_touch_area;

/* 时间校准的临时值 */

static int g_set_hour = -1;
static int g_set_minute = -1;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void med_list_rebuild(void);
static void health_refresh(void);
static void event_list_rebuild(void);
static void edit_refresh(void);
static void settings_refresh(void);

/*--------------------------------------------------------------------------
 * 小工具
 *------------------------------------------------------------------------*/

static void fmt_hm(char *buf, size_t len, time_t t)
{
  struct tm *tm = localtime(&t);

  snprintf(buf, len, "%02d:%02d", tm->tm_hour, tm->tm_min);
}

/**
 * @brief 建标签
 */

static lv_obj_t *label_make(lv_obj_t *parent, const char *text,
                            const lv_font_t *font, uint32_t color,
                            lv_align_t align, int x, int y)
{
  lv_obj_t *lbl = lv_label_create(parent);

  lv_label_set_text(lbl, text ? text : "");
  lv_obj_set_style_text_font(lbl, font, 0);
  lv_obj_set_style_text_color(lbl, lv_color_hex(color), 0);
  lv_obj_align(lbl, align, x, y);

  return lbl;
}

/**
 * @brief 建按钮（带按下反馈）
 */

static lv_obj_t *btn_make(lv_obj_t *parent, const char *text,
                          int w, int h, uint32_t bg,
                          lv_event_cb_t cb, void *user_data)
{
  lv_obj_t *btn = lv_button_create(parent);
  lv_obj_t *lbl;

  lv_obj_remove_style_all(btn);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_style_bg_color(btn, lv_color_hex(bg), 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(btn, 8, 0);
  lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);

  /* 按下时换一个更亮的底色，给用户明确的触摸反馈 */

  lv_obj_set_style_bg_color(btn, lv_color_hex(COLOR_ACCENT),
                            LV_PART_MAIN | LV_STATE_PRESSED);

  lbl = lv_label_create(btn);
  lv_label_set_text(lbl, text ? text : "");
  lv_obj_set_style_text_font(lbl, SG_FONT_TEXT, 0);
  lv_obj_set_style_text_color(lbl, lv_color_hex(COLOR_TEXT), 0);
  lv_obj_center(lbl);

  if (cb != NULL)
    {
      lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    }

  return btn;
}

/*--------------------------------------------------------------------------
 * 导航
 *------------------------------------------------------------------------*/

static void show_page(ui_page_t page)
{
  int i;

  for (i = 0; i < UI_PAGE_COUNT; i++)
    {
      if (g_page[i] == NULL)
        {
          continue;
        }

      if (i == (int)page)
        {
          lv_obj_remove_flag(g_page[i], LV_OBJ_FLAG_HIDDEN);
        }
      else
        {
          lv_obj_add_flag(g_page[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

  g_current = page;
}

static const char *page_title(ui_page_t page)
{
  switch (page)
    {
      case UI_PAGE_HOME:       return "银发守护";
      case UI_PAGE_MENU:       return "功能菜单";
      case UI_PAGE_MEDICATION: return "用药提醒";
      case UI_PAGE_MED_EDIT:   return "编辑用药";
      case UI_PAGE_HEALTH:     return "健康数据";
      case UI_PAGE_EVENTS:     return "事件记录";
      case UI_PAGE_SETTINGS:   return "设置";
      case UI_PAGE_ABOUT:      return "关于";
      case UI_PAGE_TOUCHTEST:  return "触摸自检";
      default:                 return "银发守护";
    }
}

void ui_navigate(ui_page_t page)
{
  if (page >= UI_PAGE_COUNT)
    {
      return;
    }

  if (page != g_current && g_stack_depth < 8)
    {
      g_stack[g_stack_depth++] = g_current;
    }

  /* 进入页面时刷新页面内容 */

  switch (page)
    {
      case UI_PAGE_MEDICATION: med_list_rebuild(); break;
      case UI_PAGE_MED_EDIT:   edit_refresh();     break;
      case UI_PAGE_HEALTH:     health_refresh();   break;
      case UI_PAGE_EVENTS:     event_list_rebuild(); break;
      case UI_PAGE_SETTINGS:   settings_refresh(); break;
      default: break;
    }

  show_page(page);

  lcd_set_title(page_title(page));
  lcd_set_back_visible(page != UI_PAGE_HOME);
}

void ui_back(void)
{
  if (g_stack_depth == 0)
    {
      ui_go_home();
      return;
    }

  ui_page_t prev = g_stack[--g_stack_depth];

  switch (prev)
    {
      case UI_PAGE_MEDICATION: med_list_rebuild(); break;
      case UI_PAGE_SETTINGS:   settings_refresh(); break;
      default: break;
    }

  show_page(prev);
  lcd_set_title(page_title(prev));
  lcd_set_back_visible(prev != UI_PAGE_HOME);
}

void ui_go_home(void)
{
  g_stack_depth = 0;
  show_page(UI_PAGE_HOME);
  lcd_set_title(page_title(UI_PAGE_HOME));
  lcd_set_back_visible(false);
}

ui_page_t ui_current_page(void)
{
  return g_current;
}

void ui_set_demo_mode(bool on)
{
  g_demo = on;
}

bool ui_get_demo_mode(void)
{
  return g_demo;
}

void ui_notify_user_input(void)
{
  /* 先刷新空闲计时——不然关掉演示模式后计时还是旧的，
   * lcd_task 下一轮立刻又把它打开（这个坑让整个界面都没法用）。 */

  lcd_notify_input();

  if (g_demo)
    {
      syslog(LOG_INFO, "[%s] 检测到用户输入，退出演示模式\n", LOG_TAG);
      lcd_set_demo_mode(false);
    }
}

/*--------------------------------------------------------------------------
 * 主页
 *------------------------------------------------------------------------*/

static void home_menu_cb(lv_event_t *e)
{
  (void)e;
  ui_notify_user_input();
  audio_play_tone(TONE_CLICK);
  ui_navigate(UI_PAGE_MENU);
}

static void home_sos_cb(lv_event_t *e)
{
  (void)e;
  ui_notify_user_input();
  event_trigger_sos();
}

static void build_home(lv_obj_t *page)
{
  time_t now = time(NULL);
  char buf[64];
  struct tm *tm = localtime(&now);

  g_home_clock = label_make(page, "00:00", SG_FONT_CLOCK, COLOR_TEXT,
                            LV_ALIGN_TOP_MID, 0, 4);

  snprintf(buf, sizeof(buf), "%d月%d日 星期%s",
           tm->tm_mon + 1, tm->tm_mday,
           (tm->tm_wday == 0) ? "日" : (tm->tm_wday == 1) ? "一" :
           (tm->tm_wday == 2) ? "二" : (tm->tm_wday == 3) ? "三" :
           (tm->tm_wday == 4) ? "四" : (tm->tm_wday == 5) ? "五" : "六");

  g_home_date = label_make(page, buf, SG_FONT_TEXT, COLOR_DIM,
                           LV_ALIGN_TOP_MID, 0, 58);

  {
    lv_obj_t *b1 = btn_make(page, "功能菜单", 148, 48, COLOR_CARD,
                            home_menu_cb, NULL);
    lv_obj_t *b2 = btn_make(page, "紧急求助", 148, 48, COLOR_DANGER,
                            home_sos_cb, NULL);

    lv_obj_align(b1, LV_ALIGN_BOTTOM_LEFT, 6, -6);
    lv_obj_align(b2, LV_ALIGN_BOTTOM_RIGHT, -6, -6);
  }
}

/*--------------------------------------------------------------------------
 * 功能菜单
 *------------------------------------------------------------------------*/

static void menu_entry_cb(lv_event_t *e)
{
  ui_page_t target = (ui_page_t)(intptr_t)lv_event_get_user_data(e);

  ui_notify_user_input();
  audio_play_tone(TONE_CLICK);
  ui_navigate(target);
}

/**
 * @brief 菜单里"久坐提醒"：单板没有手环的 IMU，用按钮手动触发以便演示与自测
 */

static void menu_sitting_cb(lv_event_t *e)
{
  (void)e;
  ui_notify_user_input();
  event_trigger_sitting(60 * 60);   /* 参数是秒：1 小时 */
}

/**
 * @brief 菜单里"演示模式"：手动开关页面自动轮播
 */

static void menu_demo_cb(lv_event_t *e)
{
  (void)e;
  ui_notify_user_input();
  lcd_set_demo_mode(!lcd_get_demo_mode());
}

static void build_menu(lv_obj_t *page)
{
  static const struct
  {
    const char *text;
    ui_page_t   page;
    lv_event_cb_t cb;      /* 非 NULL 时用这个回调，忽略 page */
  } entries[] =
  {
    { "用药提醒", UI_PAGE_MEDICATION, NULL            },
    { "健康数据", UI_PAGE_HEALTH,     NULL            },
    { "事件记录", UI_PAGE_EVENTS,     NULL            },
    { "设置",     UI_PAGE_SETTINGS,   NULL            },
    { "触摸自检", UI_PAGE_TOUCHTEST,  NULL            },
    { "关于",     UI_PAGE_ABOUT,      NULL            },
    { "久坐提醒", UI_PAGE_HOME,       menu_sitting_cb },
    { "演示模式", UI_PAGE_HOME,       menu_demo_cb    },
  };

  int i;

  for (i = 0; i < (int)(sizeof(entries) / sizeof(entries[0])); i++)
    {
      lv_obj_t *btn;

      if (entries[i].cb != NULL)
        {
          btn = btn_make(page, entries[i].text, 150, 38, COLOR_CARD_HI,
                         entries[i].cb, NULL);
        }
      else
        {
          btn = btn_make(page, entries[i].text, 150, 38, COLOR_CARD,
                         menu_entry_cb,
                         (void *)(intptr_t)entries[i].page);
        }

      lv_obj_set_pos(btn, (i % 2 == 0) ? 6 : 164, 2 + (i / 2) * 43);
    }
}

/*--------------------------------------------------------------------------
 * 用药提醒
 *------------------------------------------------------------------------*/

static void med_row_cb(lv_event_t *e)
{
  int index = (int)(intptr_t)lv_event_get_user_data(e);

  ui_notify_user_input();
  audio_play_tone(TONE_CLICK);

  g_edit_index = index;
  ui_navigate(UI_PAGE_MED_EDIT);
}

static void med_add_cb(lv_event_t *e)
{
  int ret;
  int hour, minute;

  (void)e;
  ui_notify_user_input();
  audio_play_tone(TONE_CLICK);

  if (medication_get_plan_count() >= MAX_MEDICATIONS)
    {
      if (g_med_hint != NULL)
        {
          lv_label_set_text(g_med_hint, "计划已满（最多 10 条），请先删除");
          lv_obj_set_style_text_color(g_med_hint,
                                      lv_color_hex(COLOR_WARN), 0);
        }

      return;
    }

  /* 新增的计划从下一个整点开始，方便立刻验证 */

  {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    hour   = (tm->tm_hour + 1) % 24;
    minute = 0;
  }

  ret = medication_add_preset(medication_get_plan_count(),
                              (uint8_t)hour, (uint8_t)minute, 1);
  if (ret != OK)
    {
      syslog(LOG_WARNING, "[%s] 新增用药计划失败: %d\n", LOG_TAG, ret);
    }

  med_list_rebuild();
}

static void med_test_cb(lv_event_t *e)
{
  const medication_plan_t *plan;

  (void)e;
  ui_notify_user_input();

  plan = medication_get_plan(0);
  if (plan != NULL)
    {
      event_trigger_medication(plan->name);
    }
  else
    {
      event_trigger_medication("测试药物");
    }
}

static void build_medication(lv_obj_t *page)
{
  g_med_hint = label_make(page, "点一下计划即可编辑", SG_FONT_TEXT, COLOR_DIM,
                          LV_ALIGN_TOP_LEFT, 8, 2);

  g_med_list = lv_obj_create(page);
  lv_obj_remove_style_all(g_med_list);
  lv_obj_set_size(g_med_list, PAGE_W - 12, 100);
  lv_obj_set_pos(g_med_list, 6, 26);
  lv_obj_set_style_bg_opa(g_med_list, LV_OPA_TRANSP, 0);
  lv_obj_set_scroll_dir(g_med_list, LV_DIR_VER);
  lv_obj_add_flag(g_med_list, LV_OBJ_FLAG_SCROLLABLE);

  {
    lv_obj_t *b1 = btn_make(page, "新增计划", 150, 40, COLOR_CARD,
                            med_add_cb, NULL);
    lv_obj_t *b2 = btn_make(page, "立即测试提醒", 150, 40, COLOR_ACCENT,
                            med_test_cb, NULL);

    lv_obj_align(b1, LV_ALIGN_BOTTOM_LEFT, 6, -4);
    lv_obj_align(b2, LV_ALIGN_BOTTOM_RIGHT, -6, -4);
  }
}

static void med_list_rebuild(void)
{
  int count;
  int i;

  if (g_med_list == NULL)
    {
      return;
    }

  lv_obj_clean(g_med_list);

  count = medication_get_plan_count();

  if (count == 0)
    {
      label_make(g_med_list, "还没有用药计划，点下面「新增计划」",
                 SG_FONT_TEXT, COLOR_DIM, LV_ALIGN_TOP_LEFT, 4, 8);
      return;
    }

  for (i = 0; i < count; i++)
    {
      const medication_plan_t *plan = medication_get_plan(i);
      lv_obj_t *row;
      lv_obj_t *lbl;
      char text[64];

      if (plan == NULL)
        {
          continue;
        }

      row = btn_make(g_med_list, "", PAGE_W - 24, 34,
                     plan->enabled ? COLOR_CARD : 0x141A22,
                     med_row_cb, (void *)(intptr_t)i);
      lv_obj_set_pos(row, 0, i * 38);

      snprintf(text, sizeof(text), "%02d:%02d  %s %d%s",
               plan->hour, plan->minute, plan->name,
               (int)plan->dosage, plan->unit);

      lbl = lv_label_create(row);
      lv_label_set_text(lbl, text);
      lv_obj_set_style_text_font(lbl, SG_FONT_TEXT, 0);
      lv_obj_set_style_text_color(lbl,
                                  lv_color_hex(plan->enabled ? COLOR_TEXT
                                                             : COLOR_DIM), 0);
      lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 8, 0);

      lbl = lv_label_create(row);
      lv_label_set_text(lbl, plan->taken ? "今日已服" :
                              (plan->enabled ? "已启用" : "已停用"));
      lv_obj_set_style_text_font(lbl, SG_FONT_TEXT, 0);
      lv_obj_set_style_text_color(lbl,
                                  lv_color_hex(plan->taken ? COLOR_OK :
                                               (plan->enabled ? COLOR_ACCENT
                                                              : COLOR_DIM)), 0);
      lv_obj_align(lbl, LV_ALIGN_RIGHT_MID, -8, 0);
    }
}

/*--------------------------------------------------------------------------
 * 编辑用药
 *------------------------------------------------------------------------*/

static void edit_refresh(void)
{
  const medication_plan_t *plan = medication_get_plan(g_edit_index);
  char buf[64];

  if (plan == NULL)
    {
      return;
    }

  if (g_edit_title != NULL)
    {
      lv_label_set_text(g_edit_title, plan->name);
    }

  if (g_edit_time != NULL)
    {
      snprintf(buf, sizeof(buf), "%02d : %02d", plan->hour, plan->minute);
      lv_label_set_text(g_edit_time, buf);
    }

  if (g_edit_dosage != NULL)
    {
      snprintf(buf, sizeof(buf), "%d %s", (int)plan->dosage, plan->unit);
      lv_label_set_text(g_edit_dosage, buf);
    }

  if (g_edit_state != NULL)
    {
      lv_label_set_text(g_edit_state, plan->enabled ? "已启用" : "已停用");
      lv_obj_set_style_text_color(g_edit_state,
                                  lv_color_hex(plan->enabled ? COLOR_OK
                                                             : COLOR_DIM), 0);
    }
}

static void edit_apply_time(int dh, int dm)
{
  const medication_plan_t *plan = medication_get_plan(g_edit_index);
  int hour;
  int minute;

  if (plan == NULL)
    {
      return;
    }

  hour   = (plan->hour + dh + 24) % 24;
  minute = (plan->minute + dm + 60) % 60;

  medication_set_time(g_edit_index, (uint8_t)hour, (uint8_t)minute);
  edit_refresh();
}

static void edit_hour_dec_cb(lv_event_t *e)
{ (void)e; ui_notify_user_input(); audio_play_tone(TONE_CLICK); edit_apply_time(-1, 0); }
static void edit_hour_inc_cb(lv_event_t *e)
{ (void)e; ui_notify_user_input(); audio_play_tone(TONE_CLICK); edit_apply_time(1, 0); }
static void edit_min_dec_cb(lv_event_t *e)
{ (void)e; ui_notify_user_input(); audio_play_tone(TONE_CLICK); edit_apply_time(0, -5); }
static void edit_min_inc_cb(lv_event_t *e)
{ (void)e; ui_notify_user_input(); audio_play_tone(TONE_CLICK); edit_apply_time(0, 5); }

static void edit_dose_dec_cb(lv_event_t *e)
{
  const medication_plan_t *plan = medication_get_plan(g_edit_index);

  (void)e;
  ui_notify_user_input();
  audio_play_tone(TONE_CLICK);

  if (plan != NULL && plan->dosage > 1)
    {
      medication_set_dosage(g_edit_index, (uint8_t)(plan->dosage - 1));
      edit_refresh();
    }
}

static void edit_dose_inc_cb(lv_event_t *e)
{
  const medication_plan_t *plan = medication_get_plan(g_edit_index);

  (void)e;
  ui_notify_user_input();
  audio_play_tone(TONE_CLICK);

  if (plan != NULL && plan->dosage < 9)
    {
      medication_set_dosage(g_edit_index, (uint8_t)(plan->dosage + 1));
      edit_refresh();
    }
}

static void edit_toggle_cb(lv_event_t *e)
{
  const medication_plan_t *plan = medication_get_plan(g_edit_index);

  (void)e;
  ui_notify_user_input();
  audio_play_tone(TONE_CLICK);

  if (plan != NULL)
    {
      medication_set_enabled(g_edit_index, !plan->enabled);
      edit_refresh();
    }
}

static void edit_delete_cb(lv_event_t *e)
{
  (void)e;
  ui_notify_user_input();
  audio_play_tone(TONE_CLICK);

  medication_remove_plan(g_edit_index);
  ui_back();
}

static void edit_confirm_cb(lv_event_t *e)
{
  (void)e;
  ui_notify_user_input();
  audio_play_tone(TONE_CLICK);

  medication_confirm_taken(g_edit_index);
  edit_refresh();
}

static void build_med_edit(lv_obj_t *page)
{
  /* 第一行：药品名 + 启用状态 */

  g_edit_title = label_make(page, "--", SG_FONT_TITLE, COLOR_TEXT,
                            LV_ALIGN_TOP_LEFT, 8, 2);

  g_edit_state = label_make(page, "--", SG_FONT_TEXT, COLOR_OK,
                            LV_ALIGN_TOP_RIGHT, -8, 8);

  /* 第二行：时间 [-] 08:00 [+] */

  label_make(page, "时间", SG_FONT_TEXT, COLOR_DIM, LV_ALIGN_TOP_LEFT, 8, 40);

  {
    lv_obj_t *b;

    b = btn_make(page, "-", 34, 30, COLOR_CARD_HI, edit_hour_dec_cb, NULL);
    lv_obj_set_pos(b, 60, 34);

    g_edit_time = label_make(page, "00 : 00", SG_FONT_TEXT, COLOR_TEXT,
                             LV_ALIGN_TOP_LEFT, 100, 40);

    b = btn_make(page, "+", 34, 30, COLOR_CARD_HI, edit_hour_inc_cb, NULL);
    lv_obj_set_pos(b, 190, 34);

    /* 分钟微调按钮放在时间右侧 */

    b = btn_make(page, "-5分", 44, 30, COLOR_CARD_HI, edit_min_dec_cb, NULL);
    lv_obj_set_pos(b, 234, 34);

    b = btn_make(page, "+5分", 44, 30, COLOR_CARD_HI, edit_min_inc_cb, NULL);
    lv_obj_set_pos(b, 234, 72);
  }

  /* 第三行：剂量 [-] 1 片 [+] */

  label_make(page, "剂量", SG_FONT_TEXT, COLOR_DIM, LV_ALIGN_TOP_LEFT, 8, 78);

  {
    lv_obj_t *b;

    b = btn_make(page, "-", 34, 30, COLOR_CARD_HI, edit_dose_dec_cb, NULL);
    lv_obj_set_pos(b, 60, 72);

    g_edit_dosage = label_make(page, "1 片", SG_FONT_TEXT, COLOR_TEXT,
                               LV_ALIGN_TOP_LEFT, 100, 78);

    b = btn_make(page, "+", 34, 30, COLOR_CARD_HI, edit_dose_inc_cb, NULL);
    lv_obj_set_pos(b, 190, 72);
  }

  /* 底部按钮 */

  {
    lv_obj_t *b;

    b = btn_make(page, "启用/停用", 90, 34, COLOR_CARD, edit_toggle_cb, NULL);
    lv_obj_set_pos(b, 6, 138);

    b = btn_make(page, "删除", 70, 34, 0x5A1A1A, edit_delete_cb, NULL);
    lv_obj_set_pos(b, 102, 138);

    b = btn_make(page, "今天已服", 90, 34, COLOR_OK, edit_confirm_cb, NULL);
    lv_obj_set_pos(b, 178, 138);
  }
}

/*--------------------------------------------------------------------------
 * 健康数据
 *------------------------------------------------------------------------*/

static void health_refresh(void)
{
  const sensors_state_t *st = sensors_get();
  char buf[32];
  int i;

  for (i = 0; i < SG_SENSOR_COUNT; i++)
    {
      if (g_health_value[i] == NULL)
        {
          continue;
        }

      if (!st->ch[i].opened)
        {
          lv_label_set_text(g_health_value[i], "--");
          lv_label_set_text(g_health_state[i], "未检测到");
          lv_obj_set_style_text_color(g_health_state[i],
                                      lv_color_hex(COLOR_DIM), 0);
          continue;
        }

      if (!st->ch[i].valid)
        {
          lv_label_set_text(g_health_value[i], "--");
          lv_label_set_text(g_health_state[i], "等待数据");
          lv_obj_set_style_text_color(g_health_state[i],
                                      lv_color_hex(COLOR_WARN), 0);
          continue;
        }

      snprintf(buf, sizeof(buf), "%.1f", (double)st->ch[i].value);
      lv_label_set_text(g_health_value[i], buf);

      snprintf(buf, sizeof(buf), "%s", st->ch[i].unit);
      lv_label_set_text(g_health_state[i], buf);
      lv_obj_set_style_text_color(g_health_state[i],
                                  lv_color_hex(COLOR_ACCENT), 0);
    }
}

static void build_health(lv_obj_t *page)
{
  const sensors_state_t *st = sensors_get();
  int i;

  for (i = 0; i < SG_SENSOR_COUNT; i++)
    {
      int y = 3 + i * 28;

      label_make(page, st->ch[i].name, SG_FONT_TEXT, COLOR_DIM,
                 LV_ALIGN_TOP_LEFT, 10, y);

      g_health_value[i] = label_make(page, "--", SG_FONT_TEXT, COLOR_TEXT,
                                     LV_ALIGN_TOP_LEFT, 90, y);

      g_health_state[i] = label_make(page, "--", SG_FONT_TEXT, COLOR_DIM,
                                     LV_ALIGN_TOP_LEFT, 200, y);
    }

  health_refresh();
}

/*--------------------------------------------------------------------------
 * 事件记录
 *------------------------------------------------------------------------*/

static void event_clear_cb(lv_event_t *e)
{
  (void)e;
  ui_notify_user_input();
  audio_play_tone(TONE_CLICK);

  event_clear_history();
  event_list_rebuild();
}

static void build_events(lv_obj_t *page)
{
  g_event_list = lv_obj_create(page);
  lv_obj_remove_style_all(g_event_list);
  lv_obj_set_size(g_event_list, PAGE_W - 12, 138);
  lv_obj_set_pos(g_event_list, 6, 2);
  lv_obj_set_style_bg_opa(g_event_list, LV_OPA_TRANSP, 0);
  lv_obj_set_scroll_dir(g_event_list, LV_DIR_VER);
  lv_obj_add_flag(g_event_list, LV_OBJ_FLAG_SCROLLABLE);

  {
    lv_obj_t *b = btn_make(page, "清空记录", 100, 30, COLOR_CARD,
                           event_clear_cb, NULL);
    lv_obj_align(b, LV_ALIGN_BOTTOM_RIGHT, -6, -4);
  }
}

static void event_list_rebuild(void)
{
  event_t history[EVENT_HISTORY_SIZE];
  int count;
  int i;

  if (g_event_list == NULL)
    {
      return;
    }

  lv_obj_clean(g_event_list);

  count = event_get_history(history, EVENT_HISTORY_SIZE);

  if (count == 0)
    {
      label_make(g_event_list, "暂无事件记录", SG_FONT_TEXT, COLOR_DIM,
                 LV_ALIGN_TOP_LEFT, 4, 8);
      return;
    }

  for (i = 0; i < count && i < 20; i++)
    {
      char buf[96];
      char hm[16];
      lv_obj_t *lbl;

      fmt_hm(hm, sizeof(hm), (time_t)history[i].timestamp);

      snprintf(buf, sizeof(buf), "%s  %s", hm,
               event_type_name(history[i].type));

      lbl = label_make(g_event_list, buf, SG_FONT_TEXT, COLOR_TEXT,
                       LV_ALIGN_TOP_LEFT, 4, i * 24);
      (void)lbl;
    }
}

/*--------------------------------------------------------------------------
 * 设置
 *------------------------------------------------------------------------*/

static void settings_save(void)
{
  audio_set_volume(g_settings.volume);
  storage_save("settings", &g_settings, sizeof(g_settings));
}

static void settings_refresh(void)
{
  char buf[32];

  if (g_set_hour < 0)
    {
      time_t now = time(NULL);
      struct tm *tm = localtime(&now);

      g_set_hour   = tm->tm_hour;
      g_set_minute = tm->tm_min;
    }

  if (g_set_volume != NULL)
    {
      snprintf(buf, sizeof(buf), "%d", (int)g_settings.volume);
      lv_label_set_text(g_set_volume, buf);
    }

  if (g_set_sit != NULL)
    {
      snprintf(buf, sizeof(buf), "%d 分钟", (int)g_settings.sit_minutes);
      lv_label_set_text(g_set_sit, buf);
    }

  if (g_set_clock_h != NULL)
    {
      snprintf(buf, sizeof(buf), "%02d 时", g_set_hour);
      lv_label_set_text(g_set_clock_h, buf);
    }

  if (g_set_clock_m != NULL)
    {
      snprintf(buf, sizeof(buf), "%02d 分", g_set_minute);
      lv_label_set_text(g_set_clock_m, buf);
    }
}

static void set_vol_dec_cb(lv_event_t *e)
{
  (void)e; ui_notify_user_input();
  if (g_settings.volume >= 10) { g_settings.volume -= 10; }
  audio_set_volume(g_settings.volume);
  audio_play_tone(TONE_CLICK);
  settings_refresh();
}
static void set_vol_inc_cb(lv_event_t *e)
{
  (void)e; ui_notify_user_input();
  if (g_settings.volume <= 90) { g_settings.volume += 10; }
  audio_set_volume(g_settings.volume);
  audio_play_tone(TONE_CLICK);
  settings_refresh();
}
static void set_sit_dec_cb(lv_event_t *e)
{
  (void)e; ui_notify_user_input(); audio_play_tone(TONE_CLICK);
  if (g_settings.sit_minutes > 10) { g_settings.sit_minutes -= 10; }
  settings_refresh();
}
static void set_sit_inc_cb(lv_event_t *e)
{
  (void)e; ui_notify_user_input(); audio_play_tone(TONE_CLICK);
  if (g_settings.sit_minutes < 240) { g_settings.sit_minutes += 10; }
  settings_refresh();
}
static void set_hour_dec_cb(lv_event_t *e)
{
  (void)e; ui_notify_user_input(); audio_play_tone(TONE_CLICK);
  g_set_hour = (g_set_hour + 23) % 24;
  settings_refresh();
}
static void set_hour_inc_cb(lv_event_t *e)
{
  (void)e; ui_notify_user_input(); audio_play_tone(TONE_CLICK);
  g_set_hour = (g_set_hour + 1) % 24;
  settings_refresh();
}
static void set_min_dec_cb(lv_event_t *e)
{
  (void)e; ui_notify_user_input(); audio_play_tone(TONE_CLICK);
  g_set_minute = (g_set_minute + 55) % 60;
  settings_refresh();
}
static void set_min_inc_cb(lv_event_t *e)
{
  (void)e; ui_notify_user_input(); audio_play_tone(TONE_CLICK);
  g_set_minute = (g_set_minute + 5) % 60;
  settings_refresh();
}

static void set_apply_cb(lv_event_t *e)
{
  struct timeval tv;
  struct tm *tm;
  time_t now;

  (void)e;
  ui_notify_user_input();

  settings_save();

  /* 时间校准：保持年月日不变，只改时分 */

  now = time(NULL);
  tm  = localtime(&now);

  tm->tm_hour = g_set_hour;
  tm->tm_min  = g_set_minute;
  tm->tm_sec  = 0;

  tv.tv_sec  = mktime(tm);
  tv.tv_usec = 0;

  if (settimeofday(&tv, NULL) == 0)
    {
      audio_play_tone(TONE_STARTUP);
      syslog(LOG_INFO, "[%s] 时间已校准为 %02d:%02d\n",
             LOG_TAG, g_set_hour, g_set_minute);
    }
  else
    {
      audio_play_tone(TONE_ERROR);
      syslog(LOG_ERR, "[%s] settimeofday 失败: %d\n", LOG_TAG, errno);
    }
}

/**
 * @brief 建一个 [-] 值 [+] 的调节行
 *
 * 按钮统一 44x36 —— 2.8 寸屏上小于 40x32 手指就很难点中，
 * 用户反馈的"触摸不灵敏"有相当一部分是目标太小而不是触摸本身的问题。
 */

static void build_adjust_row(lv_obj_t *page, int y, const char *name,
                             lv_obj_t **value_label,
                             lv_event_cb_t dec_cb, lv_event_cb_t inc_cb,
                             int value_x)
{
  lv_obj_t *b;

  label_make(page, name, SG_FONT_TEXT, COLOR_DIM, LV_ALIGN_TOP_LEFT, 6, y + 8);

  /* 高 32，行距 34 —— 留 2px 缝，相邻两行按钮不会叠在一起 */

  b = btn_make(page, "-", 44, 32, COLOR_CARD_HI, dec_cb, NULL);
  lv_obj_set_pos(b, 92, y);

  *value_label = label_make(page, "--", SG_FONT_TEXT, COLOR_TEXT,
                            LV_ALIGN_TOP_LEFT, value_x, y + 8);

  b = btn_make(page, "+", 44, 32, COLOR_CARD_HI, inc_cb, NULL);
  lv_obj_set_pos(b, 198, y);
}

static void build_settings(lv_obj_t *page)
{
  lv_obj_t *b;

  /* 四行调节，每行高 34（按钮 44x32），内容区只有 176 高，
   * 所以底部按钮放在 y=136 之后，正好占满不重叠。 */

  build_adjust_row(page, 0,   "提示音量", &g_set_volume,
                   set_vol_dec_cb, set_vol_inc_cb, 146);

  build_adjust_row(page, 34,  "久坐阈值", &g_set_sit,
                   set_sit_dec_cb, set_sit_inc_cb, 146);

  build_adjust_row(page, 68,  "时钟·时", &g_set_clock_h,
                   set_hour_dec_cb, set_hour_inc_cb, 146);

  build_adjust_row(page, 102, "时钟·分", &g_set_clock_m,
                   set_min_dec_cb, set_min_inc_cb, 146);

  b = btn_make(page, "保存并应用", 170, 38, COLOR_ACCENT, set_apply_cb, NULL);
  lv_obj_set_pos(b, 75, 136);
}

/*--------------------------------------------------------------------------
 * 关于 / 自检
 *------------------------------------------------------------------------*/

static void build_about(lv_obj_t *page)
{
  const touch_diag_t *td = touch_probe_get();
  const audio_status_t *as = audio_get_status();
  char buf[96];

  label_make(page, "银发守护 · Hub 端", SG_FONT_TEXT, COLOR_TEXT,
             LV_ALIGN_TOP_LEFT, 8, 2);

  snprintf(buf, sizeof(buf), "版本 %s", APP_VERSION);
  label_make(page, buf, SG_FONT_TEXT, COLOR_DIM, LV_ALIGN_TOP_LEFT, 8, 24);

  g_about_runtime = label_make(page, "运行 --", SG_FONT_TEXT, COLOR_DIM,
                               LV_ALIGN_TOP_LEFT, 8, 44);

  g_about_mem = label_make(page, "内存 --", SG_FONT_TEXT, COLOR_DIM,
                           LV_ALIGN_TOP_LEFT, 8, 64);

  /* 模块自检 */

  snprintf(buf, sizeof(buf), "触摸: %s  音频: %s",
           td->opened ? "就绪" : "不可用",
           as->opened ? "就绪" : "不可用");

  g_about_modules = label_make(page, buf, SG_FONT_TEXT,
                               (td->opened && as->opened) ? COLOR_OK
                                                          : COLOR_WARN,
                               LV_ALIGN_TOP_LEFT, 8, 90);

  snprintf(buf, sizeof(buf), "传感器: %d/6  存储: %s",
           sensors_get()->opened_count,
           storage_available() ? "就绪" : "不可用");

  label_make(page, buf, SG_FONT_TEXT,
             (sensors_get()->opened_count == SG_SENSOR_COUNT &&
              storage_available()) ? COLOR_OK : COLOR_WARN,
             LV_ALIGN_TOP_LEFT, 8, 110);

  g_about_storage = label_make(page, "云端: 本地桩模式", SG_FONT_TEXT,
                               COLOR_WARN, LV_ALIGN_TOP_LEFT, 8, 130);

  label_make(page, "离线运行 · 无云端依赖", SG_FONT_TEXT, COLOR_DIM,
             LV_ALIGN_TOP_LEFT, 8, 152);
}

static void about_refresh(uint32_t ticks)
{
  static uint32_t boot_ms;
  struct mallinfo mi;
  char buf[96];

  if (boot_ms == 0)
    {
      boot_ms = ticks;
      return;
    }

  if (g_about_runtime == NULL)
    {
      return;
    }

  {
    uint32_t up = (ticks - boot_ms) / 1000;

    snprintf(buf, sizeof(buf), "运行 %lu 分 %lu 秒",
             (unsigned long)(up / 60), (unsigned long)(up % 60));
    lv_label_set_text(g_about_runtime, buf);
  }

  mi = mallinfo();
  snprintf(buf, sizeof(buf), "内存 已用 %lu KB / 空闲 %lu KB",
           (unsigned long)(mi.uordblks / 1024),
           (unsigned long)(mi.fordblks / 1024));
  lv_label_set_text(g_about_mem, buf);
}

/*--------------------------------------------------------------------------
 * 触摸自检
 *------------------------------------------------------------------------*/

static void touch_reset_cb(lv_event_t *e)
{
  (void)e;
  ui_notify_user_input();
  touch_probe_reset_stats();
}

static void touch_area_cb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING)
    {
      lv_indev_t *indev = lv_indev_active();
      lv_point_t  p = { 0, 0 };

      ui_notify_user_input();

      /* 注意：这个 LVGL 版本的 lv_indev_get_point() 返回 void，
       * 不能拿返回值做判断，所以先判 indev 非空再取点。 */

      if (indev != NULL && g_touch_marker != NULL)
        {
          lv_indev_get_point(indev, &p);
          lv_obj_set_pos(g_touch_marker, p.x - 5, p.y - 5);
          lv_obj_remove_flag(g_touch_marker, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void build_touchtest(lv_obj_t *page)
{
  lv_obj_t *b;

  g_touch_stats = label_make(page, "--", SG_FONT_TEXT, COLOR_TEXT,
                             LV_ALIGN_TOP_LEFT, 6, 2);

  g_touch_lvgl = label_make(page, "--", SG_FONT_TEXT, COLOR_DIM,
                            LV_ALIGN_TOP_LEFT, 6, 24);

  label_make(page, "在下面的区域滑动，红点会跟着走", SG_FONT_TEXT, COLOR_DIM,
             LV_ALIGN_TOP_LEFT, 6, 44);

  /* 触摸可视化区域：点这里可以看到触点位置 */

  g_touch_area = lv_obj_create(page);
  lv_obj_remove_style_all(g_touch_area);
  lv_obj_set_size(g_touch_area, 250, 74);
  lv_obj_set_pos(g_touch_area, 6, 66);
  lv_obj_set_style_bg_color(g_touch_area, lv_color_hex(COLOR_CARD), 0);
  lv_obj_set_style_bg_opa(g_touch_area, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(g_touch_area, 6, 0);
  lv_obj_add_flag(g_touch_area, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(g_touch_area, touch_area_cb, LV_EVENT_PRESSED, NULL);
  lv_obj_add_event_cb(g_touch_area, touch_area_cb, LV_EVENT_PRESSING, NULL);

  g_touch_marker = lv_obj_create(g_touch_area);
  lv_obj_remove_style_all(g_touch_marker);
  lv_obj_set_size(g_touch_marker, 10, 10);
  lv_obj_set_style_bg_color(g_touch_marker, lv_color_hex(COLOR_DANGER), 0);
  lv_obj_set_style_bg_opa(g_touch_marker, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(g_touch_marker, LV_RADIUS_CIRCLE, 0);
  lv_obj_add_flag(g_touch_marker, LV_OBJ_FLAG_HIDDEN);

  b = btn_make(page, "清零", 74, 34, COLOR_CARD, touch_reset_cb, NULL);
  lv_obj_align(b, LV_ALIGN_BOTTOM_RIGHT, -6, -4);
}

static void touchtest_refresh(void)
{
  const touch_diag_t *td = touch_probe_get();
  char buf[96];

  if (g_touch_stats == NULL)
    {
      return;
    }

  if (!td->opened)
    {
      snprintf(buf, sizeof(buf), "设备不可用 errno=%d", td->err);
      lv_label_set_text(g_touch_stats, buf);
      lv_obj_set_style_text_color(g_touch_stats, lv_color_hex(COLOR_DANGER), 0);
    }
  else
    {
      snprintf(buf, sizeof(buf), "样本 %lu 按下 %lu 移动 %lu 抬起 %lu 空 %lu",
               (unsigned long)td->samples, (unsigned long)td->downs,
               (unsigned long)td->moves, (unsigned long)td->ups,
               (unsigned long)td->empty);
      lv_label_set_text(g_touch_stats, buf);
      lv_obj_set_style_text_color(g_touch_stats,
                                  lv_color_hex(td->samples > 0 ? COLOR_OK
                                                               : COLOR_WARN), 0);
    }

  snprintf(buf, sizeof(buf), "maxpoint=%u x=%d y=%d flags=0x%02x 范围 x[%d,%d] y[%d,%d]",
           td->maxpoint, (int)td->last_x, (int)td->last_y, td->last_flags,
           (int)td->min_x, (int)td->max_x, (int)td->min_y, (int)td->max_y);
  lv_label_set_text(g_touch_lvgl, buf);
}

/*--------------------------------------------------------------------------
 * 公共 tick
 *------------------------------------------------------------------------*/

static void home_refresh(void)
{
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  char buf[64];

  if (g_home_clock == NULL)
    {
      return;
    }

  snprintf(buf, sizeof(buf), "%02d:%02d", tm->tm_hour, tm->tm_min);
  lv_label_set_text(g_home_clock, buf);

  if (g_home_date != NULL)
    {
      snprintf(buf, sizeof(buf), "%d月%d日 星期%s",
               tm->tm_mon + 1, tm->tm_mday,
               (tm->tm_wday == 0) ? "日" : (tm->tm_wday == 1) ? "一" :
               (tm->tm_wday == 2) ? "二" : (tm->tm_wday == 3) ? "三" :
               (tm->tm_wday == 4) ? "四" : (tm->tm_wday == 5) ? "五" : "六");
      lv_label_set_text(g_home_date, buf);
    }
}

void ui_tick(uint32_t ticks)
{
  static uint32_t last_sec;

  if ((uint32_t)(ticks - last_sec) < 1000)
    {
      return;
    }

  last_sec = ticks;

  /* 每秒刷新的内容 */

  if (g_current == UI_PAGE_HOME)
    {
      home_refresh();
    }
  else if (g_current == UI_PAGE_TOUCHTEST)
    {
      touchtest_refresh();
    }
  else if (g_current == UI_PAGE_ABOUT)
    {
      about_refresh(ticks);
    }
  else if (g_current == UI_PAGE_HEALTH)
    {
      health_refresh();
    }

  /* 用药列表里的"今日已服"状态也要跟着变 */

  if (g_current == UI_PAGE_MEDICATION)
    {
      med_list_rebuild();
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int ui_init(lv_obj_t *content_parent)
{
  static const struct
  {
    ui_page_t page;
    void (*build)(lv_obj_t *);
  } builders[] =
  {
    { UI_PAGE_HOME,       build_home       },
    { UI_PAGE_MENU,       build_menu       },
    { UI_PAGE_MEDICATION, build_medication },
    { UI_PAGE_MED_EDIT,   build_med_edit   },
    { UI_PAGE_HEALTH,     build_health     },
    { UI_PAGE_EVENTS,     build_events     },
    { UI_PAGE_SETTINGS,   build_settings   },
    { UI_PAGE_ABOUT,      build_about      },
    { UI_PAGE_TOUCHTEST,  build_touchtest  },
  };

  unsigned i;

  if (content_parent == NULL)
    {
      return -EINVAL;
    }

  /* 页面容器：铺满内容区，黑色背景 */

  for (i = 0; i < sizeof(builders) / sizeof(builders[0]); i++)
    {
      lv_obj_t *page = lv_obj_create(content_parent);

      lv_obj_remove_style_all(page);
      lv_obj_set_size(page, PAGE_W, PAGE_H);
      lv_obj_set_pos(page, 0, 0);
      lv_obj_set_style_bg_color(page, lv_color_hex(COLOR_BG), 0);
      lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
      lv_obj_remove_flag(page, LV_OBJ_FLAG_SCROLLABLE);

      g_page[builders[i].page] = page;
      builders[i].build(page);
    }

  /* 从 storage 读回设置（没有就用默认值） */

  {
    sg_settings_t stored;

    if (storage_load("settings", &stored, sizeof(stored)) ==
        (int)sizeof(stored))
      {
        g_settings = stored;
        audio_set_volume(g_settings.volume);
        syslog(LOG_INFO, "[%s] 已恢复设置: 音量 %u 久坐 %u 分钟\n",
               LOG_TAG, g_settings.volume, g_settings.sit_minutes);
      }
  }

  /* 把状态栏的"返回"按钮接到导航栈上。
   * 之前漏了这一步，g_back_cb 一直是 NULL —— 按钮画出来了但点了没反应。 */

  lcd_set_back_callback(ui_back);

  ui_go_home();

  syslog(LOG_INFO, "[%s] 页面初始化完成（%u 页）\n",
         LOG_TAG, (unsigned)(sizeof(builders) / sizeof(builders[0])));

  return OK;
}
