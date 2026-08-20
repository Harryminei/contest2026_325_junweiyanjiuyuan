/****************************************************************************
 * Silver Guardian Band - IMU 久坐检测模块实现
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>
#include <math.h>
#include <sys/time.h>

#include "include/imu.h"
#include "include/ble.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LOG_TAG "imu"
#define IMU_BUFFER_SIZE 100
#define STEP_THRESHOLD 1.2f

/* 睡眠时段 */

#define SLEEP_START_HOUR 22
#define SLEEP_END_HOUR   7

/****************************************************************************
 * Private Data
 ****************************************************************************/

static int g_i2c_fd = -1;
static bool g_initialized = false;
static activity_status_t g_activity_status;
static sitting_event_callback_t g_sitting_callback = NULL;

/* 数据缓冲区 */

static imu_data_t g_data_buffer[IMU_BUFFER_SIZE];
static int g_buffer_index = 0;
static int g_buffer_count = 0;

/* 步数检测 */

static float g_prev_acc_magnitude = 0;
static bool g_step_peak = false;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/**
 * @brief 获取当前时间（秒）
 */

static uint32_t get_time_sec(void)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (uint32_t)tv.tv_sec;
}

/**
 * @brief 获取当前小时
 */

static int get_current_hour(void)
{
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  return tm->tm_hour;
}

/**
 * @brief 从 IMU 读取原始数据
 */

static int read_imu_raw(imu_raw_data_t *data)
{
  uint8_t buf[12];
  int ret;

  /* 读取 6 轴数据：加速度 + 陀螺仪 */

  struct i2c_msg_s msg[2];
  struct i2c_transfer_s transfer;

  /* 寄存器地址 */

  uint8_t reg = 0x28; /* OUTX_L_XL */

  msg[0].addr = IMU_I2C_ADDR;
  msg[0].flags = 0;
  msg[0].buffer = &reg;
  msg[0].length = 1;

  msg[1].addr = IMU_I2C_ADDR;
  msg[1].flags = I2C_M_READ;
  msg[1].buffer = buf;
  msg[1].length = 12;

  transfer.msgv = msg;
  transfer.msgc = 2;

  ret = ioctl(g_i2c_fd, I2C_TRANSFER, (unsigned long)&transfer);
  if (ret < 0)
    {
      return ret;
    }

  /* 解析数据 */

  data->acc_x = (int16_t)(buf[1] << 8 | buf[0]);
  data->acc_y = (int16_t)(buf[3] << 8 | buf[2]);
  data->acc_z = (int16_t)(buf[5] << 8 | buf[4]);
  data->gyro_x = (int16_t)(buf[7] << 8 | buf[6]);
  data->gyro_y = (int16_t)(buf[9] << 8 | buf[8]);
  data->gyro_z = (int16_t)(buf[11] << 8 | buf[10]);

  return OK;
}

/**
 * @brief 转换原始数据为物理单位
 */

static void convert_raw_to_data(const imu_raw_data_t *raw,
                                imu_data_t *data)
{
  /* 加速度：±2g，16-bit */

  const float acc_scale = 2.0f / 32768.0f;

  /* 陀螺仪：±250dps，16-bit */

  const float gyro_scale = 250.0f / 32768.0f;

  data->acc_x = raw->acc_x * acc_scale;
  data->acc_y = raw->acc_y * acc_scale;
  data->acc_z = raw->acc_z * acc_scale;

  data->gyro_x = raw->gyro_x * gyro_scale;
  data->gyro_y = raw->gyro_y * gyro_scale;
  data->gyro_z = raw->gyro_z * gyro_scale;

  /* 计算幅值 */

  data->acc_magnitude = sqrtf(data->acc_x * data->acc_x +
                              data->acc_y * data->acc_y +
                              data->acc_z * data->acc_z);

  data->gyro_magnitude = sqrtf(data->gyro_x * data->gyro_x +
                               data->gyro_y * data->gyro_y +
                               data->gyro_z * data->gyro_z);
}

/**
 * @brief 更新数据缓冲区
 */

static void update_buffer(const imu_data_t *data)
{
  g_data_buffer[g_buffer_index] = *data;
  g_buffer_index = (g_buffer_index + 1) % IMU_BUFFER_SIZE;

  if (g_buffer_count < IMU_BUFFER_SIZE)
    {
      g_buffer_count++;
    }
}

/**
 * @brief 计算运动特征
 */

static void calculate_features(imu_features_t *features)
{
  if (g_buffer_count == 0)
    {
      memset(features, 0, sizeof(imu_features_t));
      return;
    }

  float acc_sum = 0;
  float acc_sq_sum = 0;
  float gyro_sum = 0;
  float gyro_sq_sum = 0;
  uint16_t steps = 0;

  int count = g_buffer_count;

  for (int i = 0; i < count; i++)
    {
      acc_sum += g_data_buffer[i].acc_magnitude;
      acc_sq_sum += g_data_buffer[i].acc_magnitude *
                    g_data_buffer[i].acc_magnitude;
      gyro_sum += g_data_buffer[i].gyro_magnitude;
      gyro_sq_sum += g_data_buffer[i].gyro_magnitude *
                     g_data_buffer[i].gyro_magnitude;
    }

  features->acc_mean = acc_sum / count;
  features->acc_variance = (acc_sq_sum / count) -
                           (features->acc_mean * features->acc_mean);
  features->gyro_mean = gyro_sum / count;
  features->gyro_variance = (gyro_sq_sum / count) -
                            (features->gyro_mean * features->gyro_mean);
  features->step_count = steps;
}

/**
 * @brief 检测步数
 */

static void detect_steps(const imu_data_t *data)
{
  float magnitude = data->acc_magnitude;

  /* 简单的峰值检测算法 */

  if (magnitude > STEP_THRESHOLD && !g_step_peak)
    {
      g_step_peak = true;
    }
  else if (magnitude < 1.0f && g_step_peak)
    {
      g_step_peak = false;
      g_activity_status.total_steps++;
    }

  g_prev_acc_magnitude = magnitude;
}

/**
 * @brief 运动状态分类
 */

static motion_state_t classify_motion(const imu_features_t *features)
{
  /* 简化的分类逻辑 */

  if (features->acc_variance < STATIC_ACC_THRESHOLD &&
      features->gyro_variance < STATIC_GYRO_THRESHOLD)
    {
      return MOTION_STATIC;
    }
  else if (features->acc_mean > RUNNING_ACC_THRESHOLD)
    {
      return MOTION_RUNNING;
    }
  else if (features->acc_mean > WALKING_ACC_THRESHOLD)
    {
      return MOTION_WALKING;
    }
  else
    {
      return MOTION_OTHER;
    }
}

/**
 * @brief 更新活动状态
 */

static void update_activity_state(motion_state_t motion)
{
  uint32_t now = get_time_sec();
  bool is_sleep_time = imu_is_sleep_time();

  if (motion == MOTION_STATIC)
    {
      /* 计算静止持续时间 */

      if (g_activity_status.last_active_time > 0)
        {
          g_activity_status.still_duration =
            now - g_activity_status.last_active_time;
        }

      /* 判断活动状态 */

      if (is_sleep_time)
        {
          g_activity_status.state = ACTIVITY_SLEEPING;
        }
      else if (g_activity_status.still_duration >= STILL_THRESHOLD_SEC)
        {
          g_activity_status.state = ACTIVITY_STILL_LONG;

          if (!g_activity_status.alert_triggered)
            {
              g_activity_status.alert_triggered = true;

              if (g_sitting_callback)
                {
                  g_sitting_callback(SITTING_EVENT_ALERT);
                }
            }
        }
      else if (g_activity_status.still_duration >= SITTING_THRESHOLD_SEC)
        {
          g_activity_status.state = ACTIVITY_SITTING;

          if (!g_activity_status.reminder_triggered)
            {
              g_activity_status.reminder_triggered = true;

              if (g_sitting_callback)
                {
                  g_sitting_callback(SITTING_EVENT_REMINDER);
                }
            }
        }
    }
  else
    {
      /* 恢复活动 */

      if (g_activity_status.state != ACTIVITY_ACTIVE)
        {
          if (g_sitting_callback)
            {
              g_sitting_callback(SITTING_EVENT_RESUMED);
            }
        }

      g_activity_status.state = ACTIVITY_ACTIVE;
      g_activity_status.last_active_time = now;
      g_activity_status.still_duration = 0;
      g_activity_status.reminder_triggered = false;
      g_activity_status.alert_triggered = false;
    }

  g_activity_status.motion = motion;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int imu_init(void)
{
  syslog(LOG_INFO, "[%s] Initializing IMU module\n", LOG_TAG);

  /* 打开 I2C 设备 */

  g_i2c_fd = open("/dev/i2c0", O_RDWR);
  if (g_i2c_fd < 0)
    {
      syslog(LOG_ERR, "[%s] Failed to open I2C: %d\n", LOG_TAG, errno);
      return -errno;
    }

  /* 初始化状态 */

  memset(&g_activity_status, 0, sizeof(activity_status_t));
  g_activity_status.state = ACTIVITY_ACTIVE;
  g_activity_status.last_active_time = get_time_sec();

  g_initialized = true;

  syslog(LOG_INFO, "[%s] IMU module initialized\n", LOG_TAG);

  return OK;
}

void imu_deinit(void)
{
  if (g_i2c_fd >= 0)
    {
      close(g_i2c_fd);
      g_i2c_fd = -1;
    }

  g_initialized = false;

  syslog(LOG_INFO, "[%s] IMU module deinitialized\n", LOG_TAG);
}

void imu_update(void)
{
  if (!g_initialized)
    {
      return;
    }

  imu_raw_data_t raw;
  imu_data_t data;
  imu_features_t features;
  motion_state_t motion;

  /* 读取原始数据 */

  if (read_imu_raw(&raw) < 0)
    {
      return;
    }

  /* 转换数据 */

  convert_raw_to_data(&raw, &data);

  /* 更新缓冲区 */

  update_buffer(&data);

  /* 检测步数 */

  detect_steps(&data);

  /* 每 1 秒更新一次活动状态 */

  static uint32_t last_update = 0;
  uint32_t now = get_time_sec();

  if (now - last_update >= 1)
    {
      last_update = now;

      /* 计算特征 */

      calculate_features(&features);

      /* 分类运动状态 */

      motion = classify_motion(&features);

      /* 更新活动状态 */

      update_activity_state(motion);
    }
}

const activity_status_t *imu_get_activity_status(void)
{
  return &g_activity_status;
}

motion_state_t imu_get_motion_state(void)
{
  return g_activity_status.motion;
}

uint32_t imu_get_step_count(void)
{
  return g_activity_status.total_steps;
}

void imu_reset_step_count(void)
{
  g_activity_status.total_steps = 0;
}

void imu_register_sitting_callback(sitting_event_callback_t callback)
{
  g_sitting_callback = callback;
}

bool imu_is_sleep_time(void)
{
  int hour = get_current_hour();
  return (hour >= SLEEP_START_HOUR || hour < SLEEP_END_HOUR);
}

void imu_reset_sitting_state(void)
{
  g_activity_status.still_duration = 0;
  g_activity_status.reminder_triggered = false;
  g_activity_status.alert_triggered = false;
}

uint32_t imu_get_still_duration(void)
{
  return g_activity_status.still_duration;
}
