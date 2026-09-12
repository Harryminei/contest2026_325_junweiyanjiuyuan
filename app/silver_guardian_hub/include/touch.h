/****************************************************************************
 * Silver Guardian Hub - 触摸诊断模块
 *
 * 独立于 LVGL 直接读 /dev/input0，用于回答一个具体问题：
 * "GT911 到底有没有产生触摸事件？事件里的坐标对不对？"
 *
 * 为什么要单独探针：LVGL 的 lv_nuttx_touchscreen 只在收到合法样本时才更新
 * 状态，路径上任何一环断掉，表现都是"点了没反应"，无法区分是驱动没出事件、
 * 事件被丢弃，还是 LVGL 没接上。独立探针读同一个设备节点（NuttX 触摸上层
 * 是把事件广播给所有打开的 fd），因此不影响 LVGL，却能给出原始数据。
 ****************************************************************************/

#ifndef __APP_SILVER_GUARDIAN_HUB_INCLUDE_TOUCH_H
#define __APP_SILVER_GUARDIAN_HUB_INCLUDE_TOUCH_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdbool.h>
#include <stdint.h>

/****************************************************************************
 * Public Types
 ****************************************************************************/

typedef struct
{
  bool     probed;        /* 是否已尝试过打开设备 */
  bool     opened;        /* /dev/input0 是否打开成功 */
  int      err;           /* 失败时的 errno */
  uint8_t  maxpoint;      /* TSIOC_GETMAXPOINTS 报告的触点数上限 */
  uint32_t polls;         /* 轮询次数（心跳，用来判断探针是否在跑） */
  uint32_t samples;       /* 累计读到的样本数 */
  uint32_t downs;         /* TOUCH_DOWN 次数 */
  uint32_t moves;         /* TOUCH_MOVE 次数 */
  uint32_t ups;           /* TOUCH_UP 次数 */
  uint32_t empty;         /* 读到但无有效触点的样本数 */
  int16_t  last_x;        /* 最后一次坐标 */
  int16_t  last_y;
  uint8_t  last_flags;    /* 最后一次 flags 原始值（TOUCH_* 位） */
  int16_t  last_npoints;
  uint32_t last_ms;       /* 最后一次收到事件的时刻（毫秒） */
  int16_t  min_x;         /* 观测到的坐标范围——用来判断映射是否覆盖全屏 */
  int16_t  max_x;
  int16_t  min_y;
  int16_t  max_y;
} touch_diag_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * @brief 打开 /dev/input0 并读取 maxpoints（失败不致命，只记录 errno）
 * @return 0 表示设备可用；负值表示不可用（诊断信息仍可读）
 */

int touch_probe_init(void);

/**
 * @brief 关闭探针
 */

void touch_probe_deinit(void);

/**
 * @brief 非阻塞读取一次触摸样本（主循环调用），并记录诊断统计
 */

void touch_probe_poll(void);

/**
 * @brief 取诊断快照
 * @return 诊断结构体指针（静态，勿释放）
 */

const touch_diag_t *touch_probe_get(void);

/**
 * @brief 自上次清零以来是否收到过触摸事件
 */

bool touch_probe_has_event(void);

/**
 * @brief 清零统计（触摸自检页的"清零"按钮用）
 */

void touch_probe_reset_stats(void);

#endif /* __APP_SILVER_GUARDIAN_HUB_INCLUDE_TOUCH_H */
