/****************************************************************************
 * Silver Guardian Hub - 自检报告
 *
 * 把各模块的实际状态写到 /data/silver_guardian/diag.txt。
 *
 * 为什么需要它：板子跑起来之后，应用日志只在串口上，而板子不一定接着串口。
 * 有了这个文件，接 adb 就能直接看到"哪个设备节点没打开、errno 是多少、
 * 传感器有没有读到值、音频每步 ioctl 走到哪里失败"，不用靠猜。
 *
 *   adb shell cat /data/silver_guardian/diag.txt
 ****************************************************************************/

#ifndef __APP_SILVER_GUARDIAN_HUB_INCLUDE_DIAG_H
#define __APP_SILVER_GUARDIAN_HUB_INCLUDE_DIAG_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdbool.h>
#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* 版本与构建标记。
 *
 * 构建标记每次改动固件行为时**手动改一下**，并且必须是纯 ASCII
 * （adb shell cat 出来直接能认，也不用担心 strings 认不出中文）。
 * 用途：确认板上跑的到底是哪一版 —— 烧错镜像这件事已经浪费过一轮排查。 */

#define SG_HUB_VERSION    "1.9.0"
#define SG_HUB_BUILD_TAG  "SGHUB-BUILD-20260913-LINK"

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * @brief 生成一次自检报告并写入 /data/silver_guardian/diag.txt
 * @param reason 本次生成的原因（会写进文件头，便于分辨是开机还是定时刷新）
 * @return 0 成功
 */

int diag_dump(const char *reason);

/**
 * @brief 记录一次音频失败的具体位置（供 audio.c 调用）
 * @param stage 失败阶段名（如 "CONFIGURE"/"ALLOCBUFFER"）
 * @param err   错误码
 */

void diag_note_audio_fail(const char *stage, int err);

/**
 * @brief 取上次音频失败阶段（NULL 表示没失败过）
 */

const char *diag_audio_fail_stage(void);
int         diag_audio_fail_err(void);

#endif /* __APP_SILVER_GUARDIAN_HUB_INCLUDE_DIAG_H */
