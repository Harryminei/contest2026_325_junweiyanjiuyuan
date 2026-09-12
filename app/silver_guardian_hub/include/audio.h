/****************************************************************************
 * Silver Guardian Hub - 音频模块头文件
 *
 * 单板离线状态下没有 TTS（TTS 要联网），所以这里做的是**本地提示音**：
 * 应用启动、按键反馈、用药提醒、久坐提醒、SOS 警报各有不同的提示音，
 * 由代码直接生成 PCM 正弦波送进 NuttX 音频框架，不依赖任何音频文件。
 *
 * 播放设备：/dev/audio/pcm0p（r528_boot.c 里 audio_register("pcm0p", ...)，
 * 配合 CONFIG_AUDIO_DEV_PATH="/dev/audio"）。
 * 注意：旧版本代码写的是 /dev/i2s0，那个节点根本不存在，所以一直没声音。
 *
 * 播放放在独立线程里做，避免阻塞 LVGL 主循环（SOS 提示音有 1 秒多）。
 ****************************************************************************/

#ifndef __APP_SILVER_GUARDIAN_HUB_INCLUDE_AUDIO_H
#define __APP_SILVER_GUARDIAN_HUB_INCLUDE_AUDIO_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdbool.h>
#include <stdint.h>

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* 提示音种类 */

typedef enum
{
  TONE_CLICK = 0,     /* 按键反馈：短促一声 */
  TONE_STARTUP,       /* 启动：上行三音 */
  TONE_MEDICATION,    /* 用药提醒：叮咚两声 */
  TONE_SITTING,       /* 久坐提醒：中音两声 */
  TONE_SOS,           /* 紧急呼救：急促高音报警 */
  TONE_ERROR          /* 错误/警告：低音长鸣 */
} tone_id_t;

/* 音频状态（关于页与诊断用） */

typedef struct
{
  bool     opened;       /* 播放设备是否打开成功 */
  const char *devpath;   /* 实际使用的设备节点 */
  int      err;          /* 打开失败时的 errno */
  uint32_t played;       /* 累计播放成功次数 */
  uint32_t dropped;      /* 因忙/出错被丢弃的次数 */
  bool     playing;      /* 当前是否在播 */
  uint8_t  volume;       /* 音量 0-100（软件增益） */

  /* 下面几个用于判断"是不是被驱动缓冲截断了" */

  uint32_t buf_size;     /* 驱动给的单块缓冲大小（字节） */
  uint32_t buf_count;    /* 实际拿到的缓冲块数 */
  uint32_t last_bytes;   /* 上一次实际送出去的字节数 */
  uint32_t last_total;   /* 上一次想送的字节数（不等就是被截断） */
} audio_status_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * @brief 初始化音频（找播放设备、起播放线程）
 * @return 0 成功；负值表示音频不可用（不致命，上层继续跑）
 */

int audio_init(void);

/**
 * @brief 反初始化
 */

void audio_deinit(void);

/**
 * @brief 播放提示音（异步，立即返回）
 * @param id 提示音种类
 * @return 0 已排入播放队列；负值失败
 */

int audio_play_tone(tone_id_t id);

/**
 * @brief 兼容旧接口：原本是"播放一段 TTS 文本"
 *
 * 单板离线时没有 TTS，这里退化为播放一次用药提醒音，
 * 并把"TTS 需联网"这件事记进日志，不再假装成功。
 *
 * @param text 文本（仅用于日志）
 * @return 0 成功
 */

int audio_play(const char *text);

/**
 * @brief 播放 WAV/PCM 文件（需要板上有文件系统与音频文件）
 * @param filepath 路径
 * @return 0 成功
 */

int audio_play_file(const char *filepath);

/**
 * @brief 停止当前播放
 */

void audio_stop(void);

/**
 * @brief 设置音量（软件增益，0-100）
 */

void audio_set_volume(uint8_t volume);

/**
 * @brief 取音量
 */

uint8_t audio_get_volume(void);

/**
 * @brief 是否正在播放
 */

bool audio_is_playing(void);

/**
 * @brief 音频是否可用（设备打开成功）
 */

bool audio_is_available(void);

/**
 * @brief 取音频状态
 */

const audio_status_t *audio_get_status(void);

#endif /* __APP_SILVER_GUARDIAN_HUB_INCLUDE_AUDIO_H */
