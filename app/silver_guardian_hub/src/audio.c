/****************************************************************************
 * Silver Guardian Hub - 音频模块实现
 *
 * 流程：打开 /dev/audio/pcm0p -> AUDIOIOC_CONFIGURE 设定输出格式
 *       -> AUDIOIOC_SETBUFFERINFO/ALLOCBUFFER 拿管线缓冲
 *       -> 填 PCM -> AUDIOIOC_ENQUEUEBUFFER -> AUDIOIOC_START
 *       -> 等播完 -> AUDIOIOC_STOP -> AUDIOIOC_FREEBUFFER
 *
 * 时序参考仓库自带的 apps/system/nxplayer/nxplayer.c。
 * 所有失败都记日志并反映到 audio_status_t，不做静默吞错——旧版 audio_init
 * 打开失败还返回 OK，导致"没声音"这件事查无可查。
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <syslog.h>

#include <nuttx/audio/audio.h>

#include "include/audio.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LOG_TAG         "audio"

#define SAMPLE_RATE     16000       /* 采样率 Hz */
#define BITS_PER_SAMPLE 16
#define NCHANNELS       1

/* 播放设备候选路径。CONFIG_AUDIO_DEV_PATH="/dev/audio"，
 * r528_boot.c 里注册的设备名是 "pcm0p"。 */

static const char *g_dev_candidates[] =
{
  "/dev/audio/pcm0p",
  "/dev/pcm0p",
  "/dev/audio/pcm0p0",
  NULL
};

#define TONE_MAX_MS     2500        /* 单个提示音最长时长，用来定缓冲大小 */
#define TONE_MAX_SAMPLES (SAMPLE_RATE * TONE_MAX_MS / 1000)

/****************************************************************************
 * Private Types
 ****************************************************************************/

typedef struct
{
  uint16_t freq;      /* Hz，0 表示静音 */
  uint16_t ms;        /* 时长 */
} note_t;

/* 提示音曲谱，以 ms==0 结尾 */

static const note_t g_tone_click[]      = { {1000, 40},  {0, 0} };
static const note_t g_tone_startup[]    = { {523, 120}, {659, 120}, {784, 180}, {0, 0} };
static const note_t g_tone_medication[] = { {880, 200}, {0, 60}, {660, 260}, {0, 0} };
static const note_t g_tone_sitting[]    = { {660, 180}, {0, 80}, {660, 180}, {0, 0} };
static const note_t g_tone_error[]      = { {400, 500}, {0, 0} };
static const note_t g_tone_sos[]        =
{
  {1200, 150}, {0, 80}, {1200, 150}, {0, 80}, {1200, 150},
  {0, 200},
  {1200, 150}, {0, 80}, {1200, 150}, {0, 80}, {1200, 150},
  {0, 0}
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static audio_status_t  g_status;
static int             g_fd = -1;

static pthread_t       g_thread;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_cond = PTHREAD_COND_INITIALIZER;
static bool            g_thread_running;
static volatile bool   g_stop_requested;
static volatile int    g_pending_tone = -1;

static int16_t        *g_pcm;       /* PCM 生成缓冲（单声道 16bit） */

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static const note_t *tone_score(int id)
{
  switch (id)
    {
      case TONE_CLICK:      return g_tone_click;
      case TONE_STARTUP:    return g_tone_startup;
      case TONE_MEDICATION: return g_tone_medication;
      case TONE_SITTING:    return g_tone_sitting;
      case TONE_SOS:        return g_tone_sos;
      case TONE_ERROR:      return g_tone_error;
      default:              return g_tone_click;
    }
}

/**
 * @brief 把曲谱渲染成 PCM
 * @return 生成的采样点数（单声道）
 */

static uint32_t render_tone(const note_t *score, int16_t *out,
                            uint32_t maxsamples)
{
  uint32_t n = 0;
  uint32_t vol = g_status.volume;

  for (int i = 0; score[i].ms != 0; i++)
    {
      uint32_t count = (uint32_t)SAMPLE_RATE * score[i].ms / 1000;
      uint32_t fade  = SAMPLE_RATE / 200;   /* 5ms 淡入淡出，防爆音 */
      uint16_t freq  = score[i].freq;

      for (uint32_t k = 0; k < count && n < maxsamples; k++, n++)
        {
          if (freq == 0)
            {
              out[n] = 0;
              continue;
            }

          double amp = 12000.0 * (double)vol / 100.0;

          if (k < fade)
            {
              amp *= (double)k / (double)fade;
            }
          else if (count > fade && k > count - fade)
            {
              amp *= (double)(count - k) / (double)fade;
            }

          out[n] = (int16_t)(amp *
                             sin(2.0 * 3.14159265358979 * (double)freq *
                                 (double)k / (double)SAMPLE_RATE));
        }

      if (n >= maxsamples)
        {
          break;
        }
    }

  return n;
}

/**
 * @brief 把一段 PCM 送给音频设备播放（阻塞，由播放线程调用）
 * @return 0 成功
 */

static int device_play(const int16_t *pcm, uint32_t samples)
{
  struct audio_caps_desc_s cap_desc;
  struct ap_buffer_info_s  buf_info;
  struct audio_buf_desc_s  buf_desc;
  FAR struct ap_buffer_s  *buf[2] = { NULL, NULL };
  uint32_t total_bytes = samples * sizeof(int16_t);
  uint32_t chunk;
  uint32_t offset;
  uint32_t sent;
  uint32_t wait_ms;
  uint32_t elapsed;
  int      nbuffers;
  int      ret;
  int      i;

  /* 1. 设定输出格式：PCM / 16bit / 单声道 / 16kHz */

  memset(&cap_desc, 0, sizeof(cap_desc));
  cap_desc.caps.ac_len            = sizeof(struct audio_caps_s);
  cap_desc.caps.ac_type           = AUDIO_TYPE_OUTPUT;
  cap_desc.caps.ac_subtype        = AUDIO_FMT_PCM;
  cap_desc.caps.ac_channels       = NCHANNELS;
  cap_desc.caps.ac_chmap          = 0;
  cap_desc.caps.ac_controls.hw[0] = SAMPLE_RATE;
  cap_desc.caps.ac_controls.b[2]  = BITS_PER_SAMPLE;
  cap_desc.caps.ac_controls.b[3]  = SAMPLE_RATE >> 8;

  ret = ioctl(g_fd, AUDIOIOC_CONFIGURE, (unsigned long)&cap_desc);
  if (ret < 0)
    {
      syslog(LOG_ERR, "[%s] AUDIOIOC_CONFIGURE 失败: %d\n", LOG_TAG, errno);
      return -errno;
    }

  /* 2. 让驱动按数据量准备管线缓冲（两块） */

  memset(&buf_info, 0, sizeof(buf_info));
  buf_info.nbuffers    = 2;
  buf_info.buffer_size = total_bytes / 2 + 64;

  if (ioctl(g_fd, AUDIOIOC_SETBUFFERINFO, (unsigned long)&buf_info) != OK)
    {
      syslog(LOG_INFO, "[%s] 驱动不支持 SETBUFFERINFO，改用默认缓冲\n",
             LOG_TAG);

      if (ioctl(g_fd, AUDIOIOC_GETBUFFERINFO, (unsigned long)&buf_info) != OK)
        {
          buf_info.nbuffers    = 2;
          buf_info.buffer_size = total_bytes / 2 + 64;
        }
    }

  nbuffers = (buf_info.nbuffers >= 2) ? 2 : 1;
  chunk    = buf_info.buffer_size;
  if (chunk == 0 || chunk > total_bytes)
    {
      chunk = total_bytes;
    }

  /* 3. 分配缓冲 */

  for (i = 0; i < nbuffers; i++)
    {
      memset(&buf_desc, 0, sizeof(buf_desc));
      buf_desc.numbytes  = chunk;
      buf_desc.u.pbuffer = &buf[i];

      if (ioctl(g_fd, AUDIOIOC_ALLOCBUFFER, (unsigned long)&buf_desc)
          != (int)sizeof(buf_desc))
        {
          syslog(LOG_ERR, "[%s] ALLOCBUFFER[%d] 失败: %d\n",
                 LOG_TAG, i, errno);
          ret = -errno;
          goto out_free;
        }
    }

  /* 4. 填数据并入队 */

  offset = 0;
  for (i = 0; i < nbuffers; i++)
    {
      uint32_t bytes = (total_bytes - offset > chunk)
                       ? chunk : (total_bytes - offset);

      if (bytes == 0)
        {
          break;
        }

      memcpy(buf[i]->samp, (const uint8_t *)pcm + offset, bytes);
      buf[i]->nbytes = bytes;
      offset += bytes;

      memset(&buf_desc, 0, sizeof(buf_desc));
      buf_desc.numbytes = bytes;
      buf_desc.u.buffer = buf[i];

      ret = ioctl(g_fd, AUDIOIOC_ENQUEUEBUFFER, (unsigned long)&buf_desc);
      if (ret < 0)
        {
          syslog(LOG_ERR, "[%s] ENQUEUEBUFFER[%d] 失败: %d\n",
                 LOG_TAG, i, errno);
          ret = -errno;
          goto out_free;
        }
    }

  /* 5. 启动播放，按数据量估算时长等待 */

  ret = ioctl(g_fd, AUDIOIOC_START, 0);
  if (ret < 0)
    {
      syslog(LOG_ERR, "[%s] AUDIOIOC_START 失败: %d\n", LOG_TAG, errno);
      ret = -errno;
      goto out_free;
    }

  sent    = offset;
  wait_ms = (uint32_t)((uint64_t)sent * 1000 /
                       (SAMPLE_RATE * sizeof(int16_t))) + 150;
  elapsed = 0;

  while (elapsed < wait_ms && !g_stop_requested)
    {
      usleep(20000);
      elapsed += 20;
    }

  ioctl(g_fd, AUDIOIOC_STOP, 0);
  ret = OK;

out_free:
  for (i = 0; i < nbuffers; i++)
    {
      if (buf[i] != NULL)
        {
          memset(&buf_desc, 0, sizeof(buf_desc));
          buf_desc.numbytes = chunk;
          buf_desc.u.buffer = buf[i];
          ioctl(g_fd, AUDIOIOC_FREEBUFFER, (unsigned long)&buf_desc);
          buf[i] = NULL;
        }
    }

  return ret;
}

/**
 * @brief 播放线程：等请求 -> 生成 PCM -> 送设备
 */

static FAR void *audio_thread(FAR void *arg)
{
  (void)arg;

  while (1)
    {
      int id;

      pthread_mutex_lock(&g_lock);
      while (g_pending_tone < 0 && g_thread_running)
        {
          pthread_cond_wait(&g_cond, &g_lock);
        }

      if (!g_thread_running)
        {
          pthread_mutex_unlock(&g_lock);
          break;
        }

      id = g_pending_tone;
      g_pending_tone   = -1;
      g_status.playing = true;
      g_stop_requested = false;
      pthread_mutex_unlock(&g_lock);

      if (g_pcm != NULL)
        {
          uint32_t samples = render_tone(tone_score(id), g_pcm,
                                         TONE_MAX_SAMPLES);
          int ret = device_play(g_pcm, samples);

          if (ret < 0)
            {
              g_status.dropped++;
            }
          else
            {
              g_status.played++;
            }
        }

      g_status.playing = false;
    }

  return NULL;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int audio_init(void)
{
  const char *path = NULL;
  uint8_t volume = g_status.volume;
  int i;

  memset(&g_status, 0, sizeof(g_status));
  g_status.volume = (volume > 0 && volume <= 100) ? volume : 70;
  g_fd = -1;

  /* 找播放设备 */

  for (i = 0; g_dev_candidates[i] != NULL; i++)
    {
      if (access(g_dev_candidates[i], F_OK) == 0)
        {
          path = g_dev_candidates[i];
          break;
        }
    }

  if (path == NULL)
    {
      syslog(LOG_ERR, "[%s] 找不到播放设备，候选路径都不存在:\n", LOG_TAG);

      for (i = 0; g_dev_candidates[i] != NULL; i++)
        {
          syslog(LOG_ERR, "[%s]   %s\n", LOG_TAG, g_dev_candidates[i]);
        }

      syslog(LOG_ERR,
             "[%s] 排查: 在 NSH 执行 `ls /dev/audio` 看实际节点名\n",
             LOG_TAG);
      g_status.err = ENOENT;
      return -ENOENT;
    }

  g_fd = open(path, O_RDWR | O_CLOEXEC);
  if (g_fd < 0)
    {
      g_status.err = errno;
      syslog(LOG_ERR, "[%s] 打开 %s 失败: %d\n", LOG_TAG, path, g_status.err);
      return -g_status.err;
    }

  g_status.opened  = true;
  g_status.devpath = path;
  g_status.err     = 0;

  g_pcm = malloc(TONE_MAX_SAMPLES * sizeof(int16_t));
  if (g_pcm == NULL)
    {
      syslog(LOG_ERR, "[%s] PCM 缓冲分配失败 (%u 字节)\n", LOG_TAG,
             (unsigned)(TONE_MAX_SAMPLES * sizeof(int16_t)));
      close(g_fd);
      g_fd = -1;
      g_status.opened = false;
      return -ENOMEM;
    }

  g_thread_running = true;
  if (pthread_create(&g_thread, NULL, audio_thread, NULL) != 0)
    {
      syslog(LOG_ERR, "[%s] 播放线程创建失败: %d\n", LOG_TAG, errno);
      free(g_pcm);
      g_pcm = NULL;
      close(g_fd);
      g_fd = -1;
      g_thread_running = false;
      g_status.opened  = false;
      return -errno;
    }

  syslog(LOG_INFO, "[%s] 音频就绪: %s (%dHz/%dbit/%dch), 音量 %u\n",
         LOG_TAG, path, SAMPLE_RATE, BITS_PER_SAMPLE, NCHANNELS,
         g_status.volume);

  return OK;
}

void audio_deinit(void)
{
  if (g_thread_running)
    {
      pthread_mutex_lock(&g_lock);
      g_thread_running = false;
      g_stop_requested = true;
      pthread_cond_signal(&g_cond);
      pthread_mutex_unlock(&g_lock);
      pthread_join(g_thread, NULL);
    }

  if (g_pcm != NULL)
    {
      free(g_pcm);
      g_pcm = NULL;
    }

  if (g_fd >= 0)
    {
      close(g_fd);
      g_fd = -1;
    }

  g_status.opened  = false;
  g_status.playing = false;
}

int audio_play_tone(tone_id_t id)
{
  if (!g_status.opened || !g_thread_running || g_fd < 0)
    {
      g_status.dropped++;
      return -ENODEV;
    }

  pthread_mutex_lock(&g_lock);
  if (g_pending_tone >= 0)
    {
      /* 上一个还没开始播，直接覆盖，避免提示音堆积 */

      g_status.dropped++;
    }

  g_pending_tone = (int)id;
  pthread_cond_signal(&g_cond);
  pthread_mutex_unlock(&g_lock);

  return OK;
}

int audio_play(const char *text)
{
  /* 单板离线没有 TTS。旧实现是"调 cloud_tts 拿一段全 0 的假 buffer 再写设备"，
   * 结果是必然没声音还假装成功。现在如实降级为提示音 + 一条日志。 */

  syslog(LOG_INFO,
         "[%s] 语音播报请求(\"%s\") —— 当前离线无 TTS，降级为提示音\n",
         LOG_TAG, text ? text : "");

  return audio_play_tone(TONE_MEDICATION);
}

int audio_play_file(const char *filepath)
{
  if (filepath == NULL)
    {
      return -EINVAL;
    }

  syslog(LOG_WARNING, "[%s] 文件播放未实现: %s\n", LOG_TAG, filepath);
  return -ENOSYS;
}

void audio_stop(void)
{
  g_stop_requested = true;

  if (g_fd >= 0)
    {
      ioctl(g_fd, AUDIOIOC_STOP, 0);
    }
}

void audio_set_volume(uint8_t volume)
{
  if (volume > 100)
    {
      volume = 100;
    }

  g_status.volume = volume;
}

uint8_t audio_get_volume(void)
{
  return g_status.volume;
}

bool audio_is_playing(void)
{
  return g_status.playing;
}

bool audio_is_available(void)
{
  return g_status.opened;
}

const audio_status_t *audio_get_status(void)
{
  return &g_status;
}
