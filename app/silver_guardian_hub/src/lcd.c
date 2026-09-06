/****************************************************************************
 * Silver Guardian Hub - LCD 界面模块实现
 *
 * 基于 LVGL：
 *   - 绑定 Gemini-S1 的 /dev/lcd0（2.8寸 ILI9341 SPI 屏, 320x240）
 *   - 主界面：状态栏(时间/网络/守护状态) + 大时钟 + 日期 + 状态提示
 *   - 事件警示界面：SOS(红) / 久坐(橙) / 用药(蓝)
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

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LOG_TAG "lcd"

/* 中文字体：开启 CONFIG_LV_FONT_SIMSUN_16_CJK 时使用宋体16，否则回退 Montserrat */

#if defined(CONFIG_LV_FONT_SIMSUN_16_CJK)
#  define FONT_CJK16    (&lv_font_simsun_16_cjk)
#else
#  define FONT_CJK16    (&lv_font_montserrat_16)
#endif

#define FONT_DIGIT48    (&lv_font_montserrat_48)
#define FONT_TITLE30    (&lv_font_montserrat_30)
#define FONT_CLOCK30    (&lv_font_montserrat_30)
#define FONT_TEXT16    (&lv_font_montserrat_16)

/* 配色（RGB565 转为 lv_color_hex） */

#define COLOR_SOS_BG        0xB00000   /* 深红 */
#define COLOR_SITTING_BG    0xE06A00   /* 橙 */
#define COLOR_MEDICATION_BG 0x1E6FBA   /* 蓝 */
#define COLOR_STATUSBAR_BG  0x102030   /* 深灰蓝 */
#define COLOR_BG            0x000000   /* 黑 */
#define COLOR_WHITE         0xFFFFFF
#define COLOR_HIGH          0xE6E6E6
#define COLOR_MID           0xA8A8A8
#define COLOR_DIM           0x6E6E6E

#define ALERT_AUTO_RETURN_MS   (10 * 1000)   /* 警示界面停留上限 10s */

/****************************************************************************
 * Private Data
 ****************************************************************************/

static lv_nuttx_result_t g_result;
static bool g_initialized = false;

/* 主界面对象 */

static lv_obj_t *g_scr_main = NULL;
static lv_obj_t *g_label_clock = NULL;    /* 大时钟 */
static lv_obj_t *g_label_date = NULL;     /* 日期 */
static lv_obj_t *g_label_status = NULL;   /* 底部守护状态 */
static lv_obj_t *g_label_net = NULL;      /* 右上网络状态 */

/* 警示界面对象 */

static lv_obj_t *g_scr_alert = NULL;
static lv_obj_t *g_label_alert_title = NULL;
static lv_obj_t *g_label_alert_body = NULL;
static lv_obj_t *g_label_alert_hint = NULL;

static lcd_view_t g_view = LCD_VIEW_MAIN;
static char g_status_text[64] = "守护中";
static bool g_net_connected = true;
static int8_t g_net_signal = -50;
static uint32_t g_alert_show_ms = 0;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/* 主界面按钮——boardctl 初始化显示驱动 */

#define NEED_BOARDINIT 0

#if defined(CONFIG_BOARDCTL) && !defined(CONFIG_NSH_ARCHINIT)
#  undef  NEED_BOARDINIT
#  define NEED_BOARDINIT 1
#endif

static uint32_t current_ms(void)
{
  return (uint32_t)lv_tick_get();
}

/**
 * @brief 更新时间标签（时钟 + 日期）
 */

static void refresh_clock(void)
{
  time_t now;
  struct tm *tm;
  char buf[64];

  if (g_label_clock == NULL)
    {
      return;
    }

  now = time(NULL);
  tm = localtime(&now);

  snprintf(buf, sizeof(buf), "%02d:%02d", tm->tm_hour, tm->tm_min);
  lv_label_set_text(g_label_clock, buf);

  snprintf(buf, sizeof(buf), "%d月%d日 星期%s",
           tm->tm_mon + 1, tm->tm_mday,
           (tm->tm_wday == 0) ? "日" :
           (tm->tm_wday == 1) ? "一" :
           (tm->tm_wday == 2) ? "二" :
           (tm->tm_wday == 3) ? "三" :
           (tm->tm_wday == 4) ? "四" :
           (tm->tm_wday == 5) ? "五" : "六");
  lv_label_set_text(g_label_date, buf);
}

/**
 * @brief 刷新网络状态标签
 */

static void refresh_net(void)
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
      snprintf(buf, sizeof(buf), "未联网");
    }

  lv_label_set_text(g_label_net, buf);
}

/**
 * @brief 创建主界面
 */

static void build_main_screen(void)
{
  lv_obj_t *statusbar;
  lv_obj_t *title;

  g_scr_main = lv_obj_create(NULL);
  lv_obj_remove_style_all(g_scr_main);
  lv_obj_set_style_bg_color(g_scr_main, lv_color_hex(COLOR_BG), 0);
  lv_obj_set_style_bg_opa(g_scr_main, LV_OPA_COVER, 0);

  /* 顶栏：标题 + 网络状态 */

  statusbar = lv_obj_create(g_scr_main);
  lv_obj_remove_style_all(statusbar);
  lv_obj_set_size(statusbar, LV_PCT(100), 36);
  lv_obj_set_pos(statusbar, 0, 0);
  lv_obj_set_style_bg_color(statusbar, lv_color_hex(COLOR_STATUSBAR_BG), 0);
  lv_obj_set_style_bg_opa(statusbar, LV_OPA_COVER, 0);

  title = lv_label_create(statusbar);
  lv_label_set_text(title, "银发守护");
  lv_obj_set_style_text_font(title, FONT_CJK16, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(COLOR_WHITE), 0);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 10, 0);

  g_label_net = lv_label_create(statusbar);
  lv_obj_set_style_text_font(g_label_net, FONT_TEXT16, 0);
  lv_obj_set_style_text_color(g_label_net, lv_color_hex(COLOR_HIGH), 0);
  lv_obj_align(g_label_net, LV_ALIGN_RIGHT_MID, -10, 0);
  refresh_net();

  /* 大时钟 */

  g_label_clock = lv_label_create(g_scr_main);
  lv_obj_set_style_text_font(g_label_clock, FONT_DIGIT48, 0);
  lv_obj_set_style_text_color(g_label_clock, lv_color_hex(COLOR_WHITE), 0);
  lv_obj_align(g_label_clock, LV_ALIGN_CENTER, 0, -36);

  /* 日期 */

  g_label_date = lv_label_create(g_scr_main);
  lv_obj_set_style_text_font(g_label_date, FONT_CJK16, 0);
  lv_obj_set_style_text_color(g_label_date, lv_color_hex(COLOR_MID), 0);
  lv_obj_align(g_label_date, LV_ALIGN_CENTER, 0, 12);

  /* 底部守护状态 */

  g_label_status = lv_label_create(g_scr_main);
  lv_obj_set_style_text_font(g_label_status, FONT_TEXT16, 0);
  lv_obj_set_style_text_color(g_label_status, lv_color_hex(COLOR_HIGH), 0);
  lv_label_set_text(g_label_status, g_status_text);
  lv_obj_align(g_label_status, LV_ALIGN_BOTTOM_MID, 0, -20);

  refresh_clock();
}

/**
 * @brief 创建警示界面（复用，切换底色与文案）
 */

static void build_alert_screen(void)
{
  g_scr_alert = lv_obj_create(NULL);
  lv_obj_remove_style_all(g_scr_alert);
  lv_obj_set_style_bg_opa(g_scr_alert, LV_OPA_COVER, 0);

  g_label_alert_title = lv_label_create(g_scr_alert);
  lv_obj_set_style_text_font(g_label_alert_title, FONT_TITLE30, 0);
  lv_obj_set_style_text_color(g_label_alert_title,
                              lv_color_hex(COLOR_WHITE), 0);
  lv_obj_align(g_label_alert_title, LV_ALIGN_CENTER, 0, -60);

  g_label_alert_body = lv_label_create(g_scr_alert);
  lv_obj_set_style_text_font(g_label_alert_body, FONT_CJK16, 0);
  lv_obj_set_style_text_color(g_label_alert_body,
                              lv_color_hex(COLOR_WHITE), 0);
  lv_obj_align(g_label_alert_body, LV_ALIGN_CENTER, 0, 10);

  g_label_alert_hint = lv_label_create(g_scr_alert);
  lv_obj_set_style_text_font(g_label_alert_hint, FONT_TEXT16, 0);
  lv_obj_set_style_text_color(g_label_alert_hint,
                              lv_color_hex(COLOR_HIGH), 0);
  lv_obj_align(g_label_alert_hint, LV_ALIGN_BOTTOM_MID, 0, -24);

  g_view = LCD_VIEW_MAIN;
}

/**
 * @brief 进入警示界面
 * @param bg_color 背景色
 * @param title 标题
 * @param body 正文
 * @param view 视图类型
 */

static void show_alert(uint32_t bg_color, const char *title,
                       const char *body, lcd_view_t view)
{
  if (!g_initialized || g_scr_alert == NULL)
    {
      return;
    }

  lv_obj_set_style_bg_color(g_scr_alert, lv_color_hex(bg_color), 0);
  lv_label_set_text(g_label_alert_title, title ? title : "");
  lv_label_set_text(g_label_alert_body, body ? body : "");
  lv_label_set_text(g_label_alert_hint, "正在提醒...");

  g_view = view;
  g_alert_show_ms = current_ms();

  lv_scr_load(g_scr_alert);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int silver_lcd_init(void)
{
  lv_nuttx_dsc_t info;

  syslog(LOG_INFO, "[%s] Initializing LCD system\n", LOG_TAG);

  /* 若 LVGL 已初始化，直接复用（单一全局实例） */

  if (lv_is_initialized())
    {
      /* LVGL 已被系统初始化，复用默认 display 并接管画出界面 */
      g_result.disp = lv_display_get_default();
      if (g_result.disp == NULL)
        {
          syslog(LOG_ERR, "[%s] LVGL display attach failed!\n", LOG_TAG);
          return -ENODEV;
        }

      build_main_screen();
      build_alert_screen();
      lv_scr_load(g_scr_main);

      g_initialized = true;
      g_view = LCD_VIEW_MAIN;

      syslog(LOG_INFO, "[%s] LCD system initialized (reuse, %dx%d)\n",
             LOG_TAG,
             (int)lv_display_get_horizontal_resolution(g_result.disp),
             (int)lv_display_get_vertical_resolution(g_result.disp));

      return OK;
    }

#if NEED_BOARDINIT
  /* 执行板级显示驱动初始化 */

  boardctl(BOARDIOC_INIT, 0);
#endif

  lv_init();

  lv_nuttx_dsc_init(&info);

#ifdef CONFIG_LV_USE_NUTTX_LCD
  info.fb_path = "/dev/lcd0";
#endif

  lv_nuttx_init(&info, &g_result);

  if (g_result.disp == NULL)
    {
      syslog(LOG_ERR, "[%s] LVGL display attach failed!\n", LOG_TAG);
      return -ENODEV;
    }

  /* 创建界面 */

  build_main_screen();
  build_alert_screen();
  lv_scr_load(g_scr_main);

  g_initialized = true;
  g_view = LCD_VIEW_MAIN;

  syslog(LOG_INFO, "[%s] LCD system initialized (screen %dx%d)\n",
         LOG_TAG,
         (int)lv_display_get_horizontal_resolution(g_result.disp),
         (int)lv_display_get_vertical_resolution(g_result.disp));

  return OK;
}

void lcd_deinit(void)
{
  if (!g_initialized)
    {
      return;
    }

  lv_nuttx_deinit(&g_result);
  lv_deinit();

  g_initialized = false;
  g_label_clock = NULL;
  g_label_date = NULL;
  g_label_status = NULL;
  g_label_net = NULL;
  g_scr_main = NULL;
  g_scr_alert = NULL;

  syslog(LOG_INFO, "[%s] LCD system deinitialized\n", LOG_TAG);
}

void lcd_task(void)
{
  static uint32_t last_clock = 0;
  uint32_t now;

  if (!g_initialized)
    {
      return;
    }

  lv_timer_handler();

  now = current_ms();

  /* 每 1 秒刷新时钟 / 网络 */

  if (now - last_clock >= 1000)
    {
      last_clock = now;
      refresh_clock();
      refresh_net();
    }

  /* 警示界面超时自动返回主界面 */

  if (g_view != LCD_VIEW_MAIN &&
      now - g_alert_show_ms >= ALERT_AUTO_RETURN_MS)
    {
      lcd_clear_alert();
    }
}

void lcd_update_status(void)
{
  if (!g_initialized)
    {
      return;
    }

  refresh_clock();
  refresh_net();

  if (g_label_status != NULL && g_view == LCD_VIEW_MAIN)
    {
      lv_label_set_text(g_label_status, g_status_text);
    }
}

void lcd_show_sos(void)
{
  show_alert(COLOR_SOS_BG, "紧急求助！",
             "SOS 已发出\n正在通知家属...", LCD_VIEW_SOS);

  syslog(LOG_INFO, "[%s] SOS alert shown on screen\n", LOG_TAG);
}

void lcd_show_sitting(uint32_t minutes)
{
  char body[64];

  snprintf(body, sizeof(body), "您已静坐 %lu 分钟\n请起身活动一下",
           (unsigned long)minutes);

  show_alert(COLOR_SITTING_BG, "久坐提醒", body, LCD_VIEW_SITTING);

  syslog(LOG_INFO, "[%s] Sitting alert shown on screen\n", LOG_TAG);
}

void lcd_show_medication(const char *name, uint8_t dosage,
                         const char *unit)
{
  char body[64];

  if (dosage > 0)
    {
      snprintf(body, sizeof(body), "请服用 %s %d%s",
               name ? name : "药物", (int)dosage,
               unit ? unit : "粒");
    }
  else
    {
      snprintf(body, sizeof(body), "请服用 %s",
               name ? name : "药物");
    }

  show_alert(COLOR_MEDICATION_BG, "用药提醒", body, LCD_VIEW_MEDICATION);

  syslog(LOG_INFO, "[%s] Medication alert shown on screen\n", LOG_TAG);
}

void lcd_clear_alert(void)
{
  if (!g_initialized || g_scr_main == NULL)
    {
      return;
    }

  lv_scr_load(g_scr_main);
  g_view = LCD_VIEW_MAIN;

  lcd_update_status();
}

void lcd_set_status(const char *text)
{
  strncpy(g_status_text, text ? text : "守护中", sizeof(g_status_text) - 1);
  g_status_text[sizeof(g_status_text) - 1] = '\0';

  if (g_label_status != NULL)
    {
      lv_label_set_text(g_label_status, g_status_text);
    }
}

void lcd_set_network(bool connected, int8_t signal)
{
  g_net_connected = connected;
  g_net_signal = signal;

  if (g_label_net != NULL)
    {
      refresh_net();
    }
}

bool lcd_is_alert_active(void)
{
  return g_view != LCD_VIEW_MAIN;
}

lcd_view_t lcd_get_view(void)
{
  return g_view;
}
