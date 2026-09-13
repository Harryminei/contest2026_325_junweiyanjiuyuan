/****************************************************************************
 * Silver Guardian Hub - 状态指示灯头文件
 *
 * 硬件：板载 WS2812B 可寻址 RGB 灯（原理图 LED4，XL-3528RGBW-WS2812B），
 * 由 R528 的 LEDC 控制器驱动，NuttX 侧是标准 ws2812 驱动 /dev/leds0。
 * 写入一个 uint32 颜色值（0xRRGGBB）即可，驱动内部走 LEDC 时序。
 *
 * 走过的弯路：硬件手册写 "SYS_LED 引脚 PB10"，但板子上那颗 LED 实际不响应
 * PB10 翻转（实测常亮的只是电源指示）。真正能点亮的可见灯是这颗 WS2812，
 * 用 rgb_led 命令设成纯红验证过。
 *
 * 定位：LED 是"老人没在看屏幕时"的补充通道 —— 用颜色区分告警级别，
 * 把人引到屏幕前，再由屏幕显示大字。
 *
 * 符号一律带 sg_led_ 前缀：厂商 luncher_mini 应用里已经定义了一整族
 * led_on/led_off/led_set_color... 全局符号，本工程又是 LTO 全量链接，
 * 不带前缀会直接 multiple definition（led_off 已经撞过一次）。
 ****************************************************************************/

#ifndef __APP_SILVER_GUARDIAN_HUB_INCLUDE_LED_H
#define __APP_SILVER_GUARDIAN_HUB_INCLUDE_LED_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdbool.h>
#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* 颜色（0xRRGGBB，最高字节被驱动忽略） */

#define SG_LED_COLOR_OFF     0x000000u
#define SG_LED_COLOR_RED     0xFF0000u
#define SG_LED_COLOR_GREEN   0x00FF00u
#define SG_LED_COLOR_BLUE    0x0000FFu
#define SG_LED_COLOR_YELLOW  0xFFFF00u
#define SG_LED_COLOR_CYAN    0x00FFFFu
#define SG_LED_COLOR_MAGENTA 0xFF00FFu
#define SG_LED_COLOR_WHITE   0xFFFFFFu

/* 闪烁节奏 */

#define SG_LED_BLINK_SOLID   0   /* 常亮 */
#define SG_LED_BLINK_SLOW    1   /* 1Hz：要留意但不紧急 */
#define SG_LED_BLINK_FAST    2   /* 4Hz：紧急，最抓眼 */

/* 默认保持时长；到点自动熄灭，避免提示过期后还一直闪 */

#define SG_LED_NOTIFY_HOLD_MS  (30 * 1000)
#define SG_LED_SOS_HOLD_MS     (5 * 60 * 1000)

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* 指示灯自检状态 */

typedef struct
{
  bool        probed;      /* 是否已尝试初始化 */
  bool        opened;      /* 设备节点是否打开成功 */
  int         err;         /* 打开失败时的 errno */
  uint32_t    color;       /* 当前示警颜色 */
  uint8_t     blink;       /* 当前节奏 */
  uint32_t    writes;      /* 累计写设备的次数 */
  const char *devpath;     /* 使用的设备节点 */
} sg_led_diag_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * @brief 初始化指示灯
 * @return 0 成功，负 errno 失败（失败不致命，只是没有指示灯）
 */

int sg_led_init(void);

/**
 * @brief 反初始化（熄灭并关闭节点）
 */

void sg_led_deinit(void);

/**
 * @brief 开始一次示警
 * @param color   颜色，见 SG_LED_COLOR_*
 * @param blink   节奏，见 SG_LED_BLINK_*
 * @param hold_ms 保持时长，0 表示一直保持到显式关闭
 */

void sg_led_alert(uint32_t color, uint8_t blink, uint32_t hold_ms);

/**
 * @brief 熄灭（等价于写 SG_LED_COLOR_OFF）
 */

void sg_led_off(void);

/**
 * @brief 主循环调用：按当前颜色和节奏驱动灯
 *
 * 内部自己取时钟（clock_systime_ticks），不复用调用方的时间基准，
 * 免得和 LVGL 的 lcd_tick_ms 两个时钟域混在一起对不上。
 */

void sg_led_tick(void);

/**
 * @brief 指示灯是否可用
 */

bool sg_led_is_available(void);

/**
 * @brief 取自检状态
 */

const sg_led_diag_t *sg_led_get_diag(void);

/**
 * @brief 节奏名（自检报告显示用）
 */

const char *sg_led_blink_name(uint8_t blink);

#endif /* __APP_SILVER_GUARDIAN_HUB_INCLUDE_LED_H */
