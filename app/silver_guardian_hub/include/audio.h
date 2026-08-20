/****************************************************************************
 * Silver Guardian Hub - 音频模块头文件
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
 * Public Function Prototypes
 ****************************************************************************/

/**
 * @brief 初始化音频系统
 * @return 0 成功
 */

int audio_init(void);

/**
 * @brief 反初始化音频系统
 */

void audio_deinit(void);

/**
 * @brief 播放文本（TTS）
 * @param text 要播放的文本
 * @return 0 成功
 */

int audio_play(const char *text);

/**
 * @brief 播放音频文件
 * @param filepath 音频文件路径
 * @return 0 成功
 */

int audio_play_file(const char *filepath);

/**
 * @brief 停止播放
 */

void audio_stop(void);

/**
 * @brief 设置音量
 * @param volume 音量（0-100）
 */

void audio_set_volume(uint8_t volume);

/**
 * @brief 获取音量
 * @return 当前音量
 */

uint8_t audio_get_volume(void);

/**
 * @brief 检查是否正在播放
 * @return true 正在播放
 */

bool audio_is_playing(void);

/**
 * @brief 开始录音
 * @return 0 成功
 */

int audio_start_record(void);

/**
 * @brief 停止录音
 * @param buffer 录音数据缓冲区
 * @param size 缓冲区大小
 * @return 实际录音数据大小
 */

int audio_stop_record(uint8_t *buffer, uint32_t size);

/**
 * @brief 检查是否正在录音
 * @return true 正在录音
 */

bool audio_is_recording(void);

#endif /* __APP_SILVER_GUARDIAN_HUB_INCLUDE_AUDIO_H */
