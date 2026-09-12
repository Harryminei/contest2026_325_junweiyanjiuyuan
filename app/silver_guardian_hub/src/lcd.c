/****************************************************************************
 * Silver Guardian Hub - 显示层实现
 *
 * 屏幕：2.8 寸 ILI9341 SPI 屏，320x240，绑定 /dev/lcd0
 * 触摸：GT911 电容触摸，绑定 /dev/input0（LVGL nuttx indev）
 *
 * 布局：
 *   ┌───────────────────────────────┐  0
 *   │ [返回] 页面标题        联网状态│  32px 状态栏
 *   ├───────────────────────────────┤  32
 *   │         页面内容区 320x176    │
 *   ├───────────────────────────────┤  208
 *   │         守护状态 / 演示提示    │  32px 底栏
 *   └───────────────────────────────┘  240
 *
 * 警示层挂在 lv_layer_top() 上，不管当前在哪一页都能弹出来。
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
#include <sys/boardctl.h>

#include <lvgl/lvgl.h>

#include "include/lcd.h"
#include "include/fonts.h"
#include "include/touch.h"
#include "include/ui.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LOG_TAG "lcd"

#define SCREEN_W        320
#define SCREEN_H        240
#define STATUSBAR_H     32
#define FOOTER_H        32
#define CONTENT_Y       STATUSBAR_H
#define CONTENT_W       SCREEN_W
#define CONTENT_H       (SCREEN_H - STATUSBAR_H - FOOTER_H)

/* 配色 */

#define COLOR_STATUSBAR_BG   0x102030
#define COLOR_BG             0x000000
#define COLOR_WHITE          0xFFFFFF
#define COLOR_HIGH           0xE6E6E6
#define COLOR_MID            0xA8A8A8

#define COLOR_SOS_BG         0xB00000
#define COLOR_SITTING_BG     0xE06A00
#define COLOR_MEDICATION_BG  0x1E6FBA

#define DEMO_IDLE_MS        (30 * 1000)   /* 无输入多久后进演示模式 */
#define DEMO_PAGE_INTERVAL  (6 * 1000)    /* 演示模式每页停留 */
#define ALERT_AUTO_HIDE_MS  (15 * 1000)   /* 非 SOS 警示层自动收起 */

/****************************************************************************
 * Private Data
 ****************************************************************************/

static lv_nuttx_result_t g_result;
static bool              g_initialized;

static lv_obj_t *g_btn_back;
static lv_obj_t *g_label_title;
static lv_obj_t *g_label_net;
static lv_obj_t *g_label_footer;
static lv_obj_t *g_content;

static lv_obj_t      *g_alert_layer;
static lv_obj_t      *g_alert_title;
static lv_obj_t      *g_alert_body;
static lv_obj_t      *g_alert_kind_label;
static lcd_alert_t    g_alert_kind = LCD_ALERT_NONE;
static uint32_t       g_alert_shown_ms;
static lcd_back_cb_t  g_back_cb;

static char   g_title[32]  = "银发守护";
static char   g_footer[64] = "守护中";
static bool   g_net_connected = true;
static int8_t g_net_signal    = -50;

static bool     g_demo_mode;
static uint32_t g_last_input_ms;
static uint32_t g_last_demo_switch_ms;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void refresh_statusbar(void)
{
  char buf[32];

  if (g_label_net == NULL)
    {
      return;
    }

  if (g_net_connected)
    {
      snprintf(buf, sizeof(buf), "已联网 %d", (int)g_net_signal + 100);
    }
  else
    {
      snprintf(buf, sizeof(buf), "离线");
    }

  lv_label_set_text(g_label_net, buf);
}

static void back_btn_cb(lv_event_t *e)
{
  (void)e;

  ui_notify_user_input();

  if (g_back_cb != NULL)
    {
      g_back_cb();
    }
}

static void alert_dismiss_cb(lv_event_t *e)
{
  (void)e;

  ui_notify_user_input();
  lcd_clear_alert();
}

/**
 * @brief 建顶部状态栏
 */

static void build_statusbar(void)
{
  lv_obj_t *statusbar = lv_obj_create(lv_screen_active());
  lv_obj_remove_style_all(statusbar);
  lv_obj_set_size(statusbar, SCREEN_W, STATUSBAR_H);
  lv_obj_set_pos(statusbar, 0, 0);
  lv_obj_set_style_bg_color(statusbar, lv_color_hex(COLOR_STATUSBAR_BG), 0);
  lv_obj_set_style_bg_opa(statusbar, LV_OPA_COVER, 0);

  /* 返回按钮（只在子页面显示）
   * 尺寸从 44x24 放大到 62x28：2.8 寸屏上 44x24 大约 3.5mm x 1.9mm，
   * 手指很难点中，用户反馈"左上角那个退不出去"很大一部分是这个原因。 */

  g_btn_back = lv_button_create(statusbar);
  lv_obj_remove_style_all(g_btn_back);
  lv_obj_set_size(g_btn_back, 62, 28);
  lv_obj_align(g_btn_back, LV_ALIGN_LEFT_MID, 2, 0);
  lv_obj_set_style_bg_color(g_btn_back, lv_color_hex(0x2A3A4A), 0);
  lv_obj_set_style_bg_opa(g_btn_back, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(g_btn_back, 6, 0);
  lv_obj_add_flag(g_btn_back, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(g_btn_back, back_btn_cb, LV_EVENT_CLICKED, NULL);

  /* 按下时变色，让用户知道点到了 */

  lv_obj_set_style_bg_color(g_btn_back, lv_color_hex(0x3E6FA8),
                            LV_PART_MAIN | LV_STATE_PRESSED);

  {
    lv_obj_t *lbl = lv_label_create(g_btn_back);
    lv_label_set_text(lbl, "< 返回");
    lv_obj_set_style_text_font(lbl, SG_FONT_TEXT, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_center(lbl);
  }

  lv_obj_add_flag(g_btn_back, LV_OBJ_FLAG_HIDDEN);

  g_label_title = lv_label_create(statusbar);
  lv_label_set_text(g_label_title, g_title);
  lv_obj_set_style_text_font(g_label_title, SG_FONT_TITLE, 0);
  lv_obj_set_style_text_color(g_label_title, lv_color_hex(COLOR_WHITE), 0);
  lv_obj_align(g_label_title, LV_ALIGN_LEFT_MID, 10, 0);

  /* 必须用带中文字形的字体：这里显示的是"已联网 50"，
   * 之前误用了 SG_FONT_SMALL（Montserrat，纯 ASCII），
   * 结果"已联网"三个字在屏上是一个个方框，只有数字正常。 */

  g_label_net = lv_label_create(statusbar);
  lv_obj_set_style_text_font(g_label_net, SG_FONT_TEXT, 0);
  lv_obj_set_style_text_color(g_label_net, lv_color_hex(COLOR_HIGH), 0);
  lv_obj_align(g_label_net, LV_ALIGN_RIGHT_MID, -6, 0);

  refresh_statusbar();
}

static void build_footer(void)
{
  g_label_footer = lv_label_create(lv_screen_active());
  lv_obj_set_style_text_font(g_label_footer, SG_FONT_TEXT, 0);
  lv_obj_set_style_text_color(g_label_footer, lv_color_hex(COLOR_MID), 0);
  lv_label_set_text(g_label_footer, g_footer);
  lv_obj_align(g_label_footer, LV_ALIGN_BOTTOM_MID, 0, -6);
}

/**
 * @brief 建警示覆盖层
 */

static void build_alert_layer(void)
{
  lv_obj_t *btn;
  lv_obj_t *lbl;

  g_alert_layer = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(g_alert_layer);
  lv_obj_set_size(g_alert_layer, SCREEN_W, SCREEN_H);
  lv_obj_set_pos(g_alert_layer, 0, 0);
  lv_obj_set_style_bg_opa(g_alert_layer, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(g_alert_layer, lv_color_hex(COLOR_SOS_BG), 0);
  lv_obj_add_flag(g_alert_layer, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(g_alert_layer, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(g_alert_layer, LV_OBJ_FLAG_CLICKABLE);

  g_alert_kind_label = lv_label_create(g_alert_layer);
  lv_obj_set_style_text_font(g_alert_kind_label, SG_FONT_TEXT, 0);
  lv_obj_set_style_text_color(g_alert_kind_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(g_alert_kind_label, LV_ALIGN_TOP_MID, 0, 12);

  g_alert_title = lv_label_create(g_alert_layer);
  lv_obj_set_style_text_font(g_alert_title, SG_FONT_TITLE, 0);
  lv_obj_set_style_text_color(g_alert_title, lv_color_hex(COLOR_WHITE), 0);
  lv_obj_align(g_alert_title, LV_ALIGN_CENTER, 0, -44);

  g_alert_body = lv_label_create(g_alert_layer);
  lv_obj_set_style_text_font(g_alert_body, SG_FONT_TEXT, 0);
  lv_obj_set_style_text_color(g_alert_body, lv_color_hex(COLOR_WHITE), 0);
  lv_obj_set_style_text_align(g_alert_body, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(g_alert_body, SCREEN_W - 40);
  lv_obj_align(g_alert_body, LV_ALIGN_CENTER, 0, 8);

  btn = lv_button_create(g_alert_layer);
  lv_obj_remove_style_all(btn);
  lv_obj_set_size(btn, 150, 44);
  lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -20);
  lv_obj_set_style_bg_color(btn, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_90, 0);
  lv_obj_set_style_radius(btn, 10, 0);
  lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(btn, alert_dismiss_cb, LV_EVENT_CLICKED, NULL);

  lbl = lv_label_create(btn);
  lv_label_set_text(lbl, "知道了");
  lv_obj_set_style_text_font(lbl, SG_FONT_TITLE, 0);
  lv_obj_set_style_text_color(lbl, lv_color_hex(0x202020), 0);
  lv_obj_center(lbl);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int silver_lcd_init(void)
{
  lv_nuttx_dsc_t info;
  lv_obj_t *scr;

  syslog(LOG_INFO, "[%s] Initializing LCD system\n", LOG_TAG);

  if (lv_is_initialized())
    {
      return OK;
    }

  /* 触摸探针先起来：即使 LVGL indev 建不起来，也能拿到原始触摸数据 */

  touch_probe_init();

  lv_init();
  lv_nuttx_dsc_init(&info);

#ifdef CONFIG_LV_USE_NUTTX_LCD
  info.fb_path = "/dev/lcd0";
#endif

  lv_nuttx_init(&info, &g_result);

  if (g_result.disp == NULL)
    {
      syslog(LOG_ERR, "[%s] LVGL 显示绑定失败，界面无法显示\n", LOG_TAG);
      return -ENODEV;
    }

  scr = lv_screen_active();
  lv_obj_remove_style_all(scr);
  lv_obj_set_style_bg_color(scr, lv_color_hex(COLOR_BG), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  build_statusbar();

  /* 内容区：所有页面的父容器 */

  g_content = lv_obj_create(scr);
  lv_obj_remove_style_all(g_content);
  lv_obj_set_size(g_content, CONTENT_W, CONTENT_H);
  lv_obj_set_pos(g_content, 0, CONTENT_Y);
  lv_obj_set_style_bg_color(g_content, lv_color_hex(COLOR_BG), 0);
  lv_obj_set_style_bg_opa(g_content, LV_OPA_COVER, 0);

  build_footer();
  build_alert_layer();

  if (ui_init(g_content) != 0)
    {
      syslog(LOG_ERR, "[%s] 页面初始化失败\n", LOG_TAG);
      return -EINVAL;
    }

  g_initialized         = true;
  g_last_input_ms       = lv_tick_get();
  g_last_demo_switch_ms = g_last_input_ms;

  if (g_result.indev == NULL)
    {
      /* LVGL 没拿到触摸设备 —— 直接进演示模式，否则屏幕上什么都点不了 */

      syslog(LOG_WARNING,
             "[%s] LVGL 触摸设备不可用，进入演示模式（页面自动轮播）\n",
             LOG_TAG);
      lcd_set_demo_mode(true);
    }

  syslog(LOG_INFO, "[%s] LCD system initialized (screen %dx%d, indev=%s)\n",
         LOG_TAG,
         (int)lv_display_get_horizontal_resolution(g_result.disp),
         (int)lv_display_get_vertical_resolution(g_result.disp),
         (g_result.indev != NULL) ? "ok" : "none");

  return OK;
}

void lcd_deinit(void)
{
  if (!g_initialized)
    {
      return;
    }

  touch_probe_deinit();
  lv_nuttx_deinit(&g_result);
  lv_deinit();

  g_initialized  = false;
  g_btn_back     = NULL;
  g_label_title  = NULL;
  g_label_net    = NULL;
  g_label_footer = NULL;
  g_content      = NULL;
  g_alert_layer  = NULL;

  syslog(LOG_INFO, "[%s] LCD system deinitialized\n", LOG_TAG);
}

void lcd_task(void)
{
  static uint32_t last_clock;
  uint32_t now;

  if (!g_initialized)
    {
      return;
    }

  touch_probe_poll();
  lv_timer_handler();

  now = lv_tick_get();

  if ((uint32_t)(now - last_clock) >= 1000)
    {
      last_clock = now;
      refresh_statusbar();
    }

  /* 自动进演示模式，但**只在触摸设备根本没产生过任何事件时**才做。
   *
   * 这里踩过一个坑：原来只判断"多久没有用户输入"，而 g_last_input_ms 只在
   * 开机时赋过一次值，用户触摸时没刷新。结果是开机 30 秒后条件恒为真，
   * 用户点一下关掉演示模式，下一轮循环立刻又打开 —— 页面自己乱跳，什么都干不了。
   * 现在加两道闸：触摸探针见过事件就永不自动进；且每次用户输入都刷新计时。 */

  if (!g_demo_mode &&
      touch_probe_get()->samples == 0 &&
      (uint32_t)(now - g_last_input_ms) >= DEMO_IDLE_MS)
    {
      syslog(LOG_WARNING,
             "[%s] %u 秒内触摸设备无任何事件，自动进入演示模式\n",
             LOG_TAG, (unsigned)(DEMO_IDLE_MS / 1000));
      lcd_set_demo_mode(true);
    }

  if (g_demo_mode &&
      (uint32_t)(now - g_last_demo_switch_ms) >= DEMO_PAGE_INTERVAL)
    {
      g_last_demo_switch_ms = now;
      ui_navigate((ui_page_t)((ui_current_page() + 1) % UI_PAGE_COUNT));
    }

  /* 非 SOS 的警示层超时自动收起 */

  if (g_alert_kind != LCD_ALERT_NONE &&
      g_alert_kind != LCD_ALERT_SOS &&
      (uint32_t)(now - g_alert_shown_ms) >= ALERT_AUTO_HIDE_MS)
    {
      lcd_clear_alert();
    }
}

/*--------------------------------------------------------------------------
 * 状态栏
 *------------------------------------------------------------------------*/

void lcd_set_title(const char *text)
{
  if (text == NULL)
    {
      return;
    }

  strncpy(g_title, text, sizeof(g_title) - 1);
  g_title[sizeof(g_title) - 1] = '\0';

  if (g_label_title != NULL)
    {
      lv_label_set_text(g_label_title, g_title);
    }
}

void lcd_set_back_visible(bool visible)
{
  if (g_btn_back == NULL)
    {
      return;
    }

  if (visible)
    {
      lv_obj_remove_flag(g_btn_back, LV_OBJ_FLAG_HIDDEN);
    }
  else
    {
      lv_obj_add_flag(g_btn_back, LV_OBJ_FLAG_HIDDEN);
    }

  if (g_label_title != NULL)
    {
      /* 返回按钮宽度 62 + 左边距 2，标题从 68 开始才不重叠 */

      lv_obj_align(g_label_title, LV_ALIGN_LEFT_MID, visible ? 68 : 10, 0);
    }
}

void lcd_set_back_callback(lcd_back_cb_t cb)
{
  g_back_cb = cb;
}

void lcd_set_network(bool connected, int8_t signal)
{
  g_net_connected = connected;
  g_net_signal    = signal;
  refresh_statusbar();
}

void lcd_set_guard_status(const char *text)
{
  if (text == NULL)
    {
      return;
    }

  strncpy(g_footer, text, sizeof(g_footer) - 1);
  g_footer[sizeof(g_footer) - 1] = '\0';

  if (g_label_footer != NULL)
    {
      lv_label_set_text(g_label_footer, g_footer);
    }
}

/*--------------------------------------------------------------------------
 * 警示层
 *------------------------------------------------------------------------*/

void lcd_show_alert(lcd_alert_t kind, const char *title, const char *body)
{
  uint32_t bg;
  const char *kind_text;

  if (!g_initialized || g_alert_layer == NULL)
    {
      return;
    }

  switch (kind)
    {
      case LCD_ALERT_SOS:
        bg = COLOR_SOS_BG;
        kind_text = "紧急情况";
        break;

      case LCD_ALERT_SITTING:
        bg = COLOR_SITTING_BG;
        kind_text = "健康提醒";
        break;

      case LCD_ALERT_MEDICATION:
        bg = COLOR_MEDICATION_BG;
        kind_text = "用药提醒";
        break;

      default:
        bg = COLOR_STATUSBAR_BG;
        kind_text = "";
        break;
    }

  lv_obj_set_style_bg_color(g_alert_layer, lv_color_hex(bg), 0);
  lv_label_set_text(g_alert_kind_label, kind_text);
  lv_label_set_text(g_alert_title, title ? title : "");
  lv_label_set_text(g_alert_body, body ? body : "");

  g_alert_kind     = kind;
  g_alert_shown_ms = lv_tick_get();

  lv_obj_remove_flag(g_alert_layer, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(g_alert_layer);

  syslog(LOG_INFO, "[%s] 警示层弹出: kind=%d title=%s\n",
         LOG_TAG, (int)kind, title ? title : "");
}

void lcd_clear_alert(void)
{
  if (g_alert_layer == NULL)
    {
      return;
    }

  lv_obj_add_flag(g_alert_layer, LV_OBJ_FLAG_HIDDEN);
  g_alert_kind = LCD_ALERT_NONE;
}

bool lcd_is_alert_active(void)
{
  return g_alert_kind != LCD_ALERT_NONE;
}

/*--------------------------------------------------------------------------
 * 触摸与演示模式
 *------------------------------------------------------------------------*/

bool lcd_touch_ready(void)
{
  return g_result.indev != NULL;
}

void lcd_set_demo_mode(bool on)
{
  g_demo_mode = on;
  ui_set_demo_mode(on);

  /* 不管开还是关都刷新空闲计时：关掉演示模式后若计时不刷新，
   * 下一轮循环会立刻又判定"空闲超时"把它打开。 */

  g_last_input_ms = lv_tick_get();

  if (on)
    {
      g_last_demo_switch_ms = lv_tick_get();
    }

  lcd_set_guard_status(on ? "演示模式 · 触摸屏幕退出" : "守护中");
}

bool lcd_get_demo_mode(void)
{
  return g_demo_mode;
}

void lcd_notify_input(void)
{
  g_last_input_ms = lv_tick_get();
}

uint32_t lcd_tick_ms(void)
{
  return (uint32_t)lv_tick_get();
}
