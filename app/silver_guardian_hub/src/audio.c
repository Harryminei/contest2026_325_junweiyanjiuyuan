/****************************************************************************
 * Silver Guardian Hub - 音频模块实现
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>

#include "include/audio.h"
#include "include/cloud.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LOG_TAG "audio"
#define I2S_DEV "/dev/i2s0"
#define DEFAULT_VOLUME 70

/****************************************************************************
 * Private Data
 ****************************************************************************/

static int g_i2s_fd = -1;
static uint8_t g_volume = DEFAULT_VOLUME;
static bool g_playing = false;
static bool g_recording = false;
static bool g_initialized = false;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int audio_init(void)
{
  syslog(LOG_INFO, "[%s] Initializing audio system\n", LOG_TAG);

  /* 打开 I2S 设备 */

  g_i2s_fd = open(I2S_DEV, O_WRONLY);
  if (g_i2s_fd < 0)
    {
      syslog(LOG_WARNING, "[%s] I2S not available: %d\n",
             LOG_TAG, errno);
    }

  g_initialized = true;

  syslog(LOG_INFO, "[%s] Audio system initialized\n", LOG_TAG);

  return OK;
}

void audio_deinit(void)
{
  audio_stop();

  if (g_i2s_fd >= 0)
    {
      close(g_i2s_fd);
      g_i2s_fd = -1;
    }

  g_initialized = false;

  syslog(LOG_INFO, "[%s] Audio system deinitialized\n", LOG_TAG);
}

int audio_play(const char *text)
{
  if (!g_initialized)
    {
      return -ENODEV;
    }

  syslog(LOG_INFO, "[%s] Playing TTS: %s\n", LOG_TAG, text);

  /* 调用云端 TTS 服务 */

  audio_buffer_t *buffer = cloud_tts(text);
  if (buffer == NULL)
    {
      syslog(LOG_ERR, "[%s] TTS failed\n", LOG_TAG);
      return -EIO;
    }

  /* 播放音频 */

  g_playing = true;

  if (g_i2s_fd >= 0)
    {
      size_t bytes_written;
      write(g_i2s_fd, buffer->data, buffer->size);
    }

  /* 等待播放完成 */

  usleep(100000);  /* 简化处理 */

  g_playing = false;

  /* 释放缓冲区 */

  free_audio_buffer(buffer);

  syslog(LOG_INFO, "[%s] Playback complete\n", LOG_TAG);

  return OK;
}

int audio_play_file(const char *filepath)
{
  if (!g_initialized)
    {
      return -ENODEV;
    }

  syslog(LOG_INFO, "[%s] Playing file: %s\n", LOG_TAG, filepath);

  /* TODO: 实际的文件播放 */

  return OK;
}

void audio_stop(void)
{
  if (g_i2s_fd >= 0)
    {
      /* 清空 DMA 缓冲区 */

      /* TODO: 实际的停止代码 */
    }

  g_playing = false;
  g_recording = false;
}

void audio_set_volume(uint8_t volume)
{
  if (volume > 100)
    {
      volume = 100;
    }

  g_volume = volume;

  syslog(LOG_INFO, "[%s] Volume set to: %d\n", LOG_TAG, g_volume);

  /* TODO: 实际的音量设置 */
}

uint8_t audio_get_volume(void)
{
  return g_volume;
}

bool audio_is_playing(void)
{
  return g_playing;
}

int audio_start_record(void)
{
  if (!g_initialized)
    {
      return -ENODEV;
    }

  syslog(LOG_INFO, "[%s] Start recording\n", LOG_TAG);

  g_recording = true;

  /* TODO: 实际的录音开始 */

  return OK;
}

int audio_stop_record(uint8_t *buffer, uint32_t size)
{
  if (!g_recording)
    {
      return -EPERM;
    }

  syslog(LOG_INFO, "[%s] Stop recording\n", LOG_TAG);

  g_recording = false;

  /* TODO: 实际的录音停止和数据获取 */

  return 0;
}

bool audio_is_recording(void)
{
  return g_recording;
}
