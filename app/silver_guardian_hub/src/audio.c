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
#include "include/diag.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LOG_TAG         "audio"

/* 8000Hz 就够：提示音最高 1200Hz，奈奎斯特 4kHz 富余；
 * 采样率减半意味着同样时长的 PCM 数据量减半，更容易装进驱动的管线缓冲。
 * （原来 16kHz 时 1.5 秒的 SOS 要 48KB，驱动的缓冲装不下会被截断） */

#define SAMPLE_RATE     8000        /* 采样率 Hz */
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
/* SOS 总时长压到约 1 秒：再长会因为超出驱动的管线缓冲而被截断 */

static const note_t g_tone_sos[]        =
{
  {1200, 120}, {0, 60}, {1200, 120}, {0, 60}, {1200, 120},
  {0, 160},
  {1200, 120}, {0, 60}, {1200, 120},
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
      diag_note_audio_fail("AUDIOIOC_CONFIGURE", errno);
      return -errno;
    }

  /* 2. 问驱动要它的首选缓冲参数，**不去改它**。
   *
   * 这里原来会先 AUDIOIOC_SETBUFFERINFO 把缓冲改大以容纳整段提示音，
   * 但驱动不一定能满足更大的连续内存请求，失败后反而连缓冲都拿不到。
   * 改成完全听驱动的：它给多大就用多大，装不下的部分截断（会记日志）。
   * 先确保能出声，再谈音质。 */

  memset(&buf_info, 0, sizeof(buf_info));

  if (ioctl(g_fd, AUDIOIOC_GETBUFFERINFO, (unsigned long)&buf_info) != OK ||
      buf_info.nbuffers == 0 || buf_info.buffer_size == 0)
    {
      syslog(LOG_WARNING,
             "[%s] GETBUFFERINFO 不可用，用保守默认值 (2 x 4096)\n", LOG_TAG);
      buf_info.nbuffers    = 2;
      buf_info.buffer_size = 4096;
    }

  syslog(LOG_INFO, "[%s] 驱动缓冲: %lu 块 x %lu 字节, 本次要送 %lu 字节\n",
         LOG_TAG, (unsigned long)buf_info.nbuffers,
         (unsigned long)buf_info.buffer_size, (unsigned long)total_bytes);

  nbuffers = (buf_info.nbuffers >= 2) ? 2 : 1;
  chunk    = buf_info.buffer_size;
  if (chunk > total_bytes)
    {
      chunk = total_bytes;
    }

  /* 3. 分配缓冲
   *
   * 判据是"驱动有没有把指针填上"，**不是** ioctl 的返回值。
   * 这套实现里 audio_allocbuffer() 在缓冲池用完时返回 0（不是错误），
   * 而 nxplayer 那种 `!= sizeof(buf_desc)` 的判据会把这种情况误判成失败，
   * 结果一块缓冲都拿不到、直接放弃播放 —— 板子上"没声音"就是这么来的
   * （自检报告里那句 AUDIOIOC_ALLOCBUFFER 失败 errno=0 就是它）。
   * 拿不到更多就用已经拿到的那些，能播多少播多少。 */

  {
    int got = 0;

    for (i = 0; i < nbuffers; i++)
      {
        buf[i] = NULL;
        memset(&buf_desc, 0, sizeof(buf_desc));
        buf_desc.numbytes  = chunk;
        buf_desc.u.pbuffer = &buf[i];

        ret = ioctl(g_fd, AUDIOIOC_ALLOCBUFFER, (unsigned long)&buf_desc);
        if (ret < 0)
          {
            syslog(LOG_ERR, "[%s] ALLOCBUFFER[%d] 出错: ret=%d errno=%d\n",
                   LOG_TAG, i, ret, errno);
            diag_note_audio_fail("AUDIOIOC_ALLOCBUFFER", errno ? errno : ret);
            ret = -errno;
            goto out_free;
          }

        if (buf[i] == NULL)
          {
            /* 缓冲池已满：不是错误，用已经拿到的就行 */

            syslog(LOG_INFO, "[%s] 缓冲池只给了 %d 块（请求 %d 块）\n",
                   LOG_TAG, got, nbuffers);
            break;
          }

        got++;
      }

    if (got == 0)
      {
        syslog(LOG_ERR, "[%s] 一块缓冲都没拿到，无法播放\n", LOG_TAG);
        diag_note_audio_fail("AUDIOIOC_ALLOCBUFFER(0)", errno);
        ret = -ENOMEM;
        goto out_free;
      }

    nbuffers = got;
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
          syslog(LOG_ERR, "[%s] ENQUEUEBUFFER[%d] 出错: ret=%d errno=%d\n",
                 LOG_TAG, i, ret, errno);
          diag_note_audio_fail("AUDIOIOC_ENQUEUEBUFFER", errno ? errno : ret);
          ret = -errno;
          goto out_free;
        }
    }

  /* 5. 启动播放，按数据量估算时长等待 */

  ret = ioctl(g_fd, AUDIOIOC_START, 0);
  if (ret < 0)
    {
      syslog(LOG_ERR, "[%s] AUDIOIOC_START 出错: ret=%d errno=%d\n",
             LOG_TAG, ret, errno);
      diag_note_audio_fail("AUDIOIOC_START", errno ? errno : ret);
      ret = -errno;
      goto out_free;
    }

  sent    = offset;

  /* 记下"想送多少"和"实送多少"，不等就是被驱动的管线缓冲截断了 */

  g_status.buf_size   = chunk;
  g_status.buf_count  = (uint32_t)nbuffers;
  g_status.last_bytes = sent;
  g_status.last_total = total_bytes;

  if (sent < total_bytes)
    {
      syslog(LOG_WARNING,
             "[%s] 数据被截断: 想送 %lu 字节, 实送 %lu（单块 %lu x %d 块）\n",
             LOG_TAG, (unsigned long)total_bytes, (unsigned long)sent,
             (unsigned long)chunk, nbuffers);
    }

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
