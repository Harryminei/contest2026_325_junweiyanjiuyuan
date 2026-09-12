/****************************************************************************
 * Silver Guardian Hub - 字体声明
 *
 * 字库由 tools/gen_lvgl_fonts.sh 生成（源字体 Noto Sans SC, SIL OFL 1.1）：
 *   sg_font_16  16px 4bpp  GB2312 一级字库(3755 汉字) + ASCII + 中文标点
 *   sg_font_24  24px 2bpp  同上
 *
 * 为什么要自己生成：LVGL 自带的 lv_font_simsun_16_cjk 只是示例字库，
 * CJK 部分仅 1166 个字形，"银发守护"里只有"守"字在里面。
 ****************************************************************************/

#ifndef __APP_SILVER_GUARDIAN_HUB_INCLUDE_FONTS_H
#define __APP_SILVER_GUARDIAN_HUB_INCLUDE_FONTS_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <lvgl/lvgl.h>

/****************************************************************************
 * Public Data
 ****************************************************************************/

LV_FONT_DECLARE(sg_font_16);
LV_FONT_DECLARE(sg_font_24);

/* 语义化别名：改了字号只动这里 */

#define SG_FONT_TEXT    (&sg_font_16)             /* 正文 / 列表 / 按钮 */
#define SG_FONT_TITLE   (&sg_font_24)             /* 页面标题 */
#define SG_FONT_CLOCK   (&lv_font_montserrat_48)  /* 大时钟数字（纯数字） */

/* ⚠️ Montserrat 系列**没有中文字形**，只能用于纯数字/英文。
 * 拿它显示中文，屏幕上会出现一个个方框（踩过：状态栏的"已联网 50"
 * 曾误用它，结果"已联网"三个字全变方框）。中文一律用 SG_FONT_TEXT。 */

#define SG_FONT_ASCII_ONLY (&lv_font_montserrat_16)

#endif /* __APP_SILVER_GUARDIAN_HUB_INCLUDE_FONTS_H */
