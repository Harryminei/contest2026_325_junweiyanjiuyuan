/****************************************************************************
 * Silver Guardian Band - IMU 久坐检测模块头文件
 ****************************************************************************/

#ifndef __APP_SILVER_GUARDIAN_BAND_INCLUDE_IMU_H
#define __APP_SILVER_GUARDIAN_BAND_INCLUDE_IMU_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdbool.h>
#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* IMU 配置 */

#define IMU_SAMPLE_RATE_HZ      100   /* 采样率 100Hz */
#define IMU_I2C_ADDR            0x6A  /* LSM6DSO I2C 地址 */

/* 久坐检测阈值 */

#define SITTING_THRESHOLD_SEC   (60 * 60)     /* 60 分钟 */
#define STILL_THRESHOLD_SEC     (2 * 60 * 60) /* 2 小时 */

/* 运动检测阈值 */

#define STATIC_ACC_THRESHOLD    0.5f   /* 静止加速度阈值 (g) */
#define STATIC_GYRO_THRESHOLD   5.0f   /* 静止陀螺仪阈值 (dps) */
#define WALKING_ACC_THRESHOLD   1.5f   /* 步行加速度阈值 (g) */
#define RUNNING_ACC_THRESHOLD   3.0f   /* 跑步加速度阈值 (g) */

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* 运动状态 */

typedef enum
{
  MOTION_STATIC = 0,     /* 静止 */
  MOTION_WALKING,        /* 步行 */
  MOTION_RUNNING,        /* 跑步 */
  MOTION_OTHER           /* 其他运动 */
} motion_state_t;

/* 活动状态 */

typedef enum
{
  ACTIVITY_ACTIVE = 0,   /* 活动状态 */
  ACTIVITY_SITTING,      /* 久坐状态 */
  ACTIVITY_STILL_LONG,   /* 长时间静止 */
  ACTIVITY_SLEEPING      /* 睡眠状态 */
} activity_state_t;

/* IMU 原始数据 */

typedef struct
{
  int16_t acc_x;         /* X 轴加速度 */
  int16_t acc_y;         /* Y 轴加速度 */
  int16_t acc_z;         /* Z 轴加速度 */
  int16_t gyro_x;        /* X 轴角速度 */
  int16_t gyro_y;        /* Y 轴角速度 */
  int16_t gyro_z;        /* Z 轴角速度 */
} imu_raw_data_t;

/* IMU 处理后数据 */

typedef struct
{
  float acc_x;           /* X 轴加速度 (g) */
  float acc_y;           /* Y 轴加速度 (g) */
  float acc_z;           /* Z 轴加速度 (g) */
  float gyro_x;          /* X 轴角速度 (dps) */
  float gyro_y;          /* Y 轴角速度 (dps) */
  float gyro_z;          /* Z 轴角速度 (dps) */
  float acc_magnitude;   /* 加速度幅值 */
  float gyro_magnitude;  /* 陀螺仪幅值 */
} imu_data_t;

/* 运动特征 */

typedef struct
{
  float acc_mean;        /* 加速度均值 */
  float acc_variance;    /* 加速度方差 */
  float gyro_mean;       /* 陀螺仪均值 */
  float gyro_variance;   /* 陀螺仪方差 */
  uint16_t step_count;   /* 步数 */
} imu_features_t;

/* 活动状态结构体 */

typedef struct
{
  activity_state_t state;    /* 当前状态 */
  motion_state_t motion;     /* 运动状态 */
  uint32_t still_duration;   /* 静止持续时间 (秒) */
  uint32_t total_steps;      /* 总步数 */
  uint32_t last_active_time; /* 最后活动时间 */
  bool reminder_triggered;   /* 提醒已触发 */
  bool alert_triggered;      /* 告警已触发 */
} activity_status_t;

/* 久坐事件类型 */

typedef enum
{
  SITTING_EVENT_NONE = 0,
  SITTING_EVENT_REMINDER,    /* 久坐提醒 */
  SITTING_EVENT_ALERT,       /* 异常告警 */
  SITTING_EVENT_RESUMED      /* 恢复活动 */
} sitting_event_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * @brief 初始化 IMU 模块
 * @return 0 成功，负数失败
 */

int imu_init(void);

/**
 * @brief 反初始化 IMU 模块
 */

void imu_deinit(void);

/**
 * @brief 更新 IMU 数据（主循环调用）
 */

void imu_update(void);

/**
 * @brief 获取当前活动状态
 * @return 活动状态结构体指针
 */

const activity_status_t *imu_get_activity_status(void);

/**
 * @brief 获取当前运动状态
 * @return 运动状态
 */

motion_state_t imu_get_motion_state(void);

/**
 * @brief 获取步数
 * @return 总步数
 */

uint32_t imu_get_step_count(void);

/**
 * @brief 重置步数
 */

void imu_reset_step_count(void);

/**
 * @brief 注册久坐事件回调
 * @param callback 回调函数
 */

typedef void (*sitting_event_callback_t)(sitting_event_t event);
void imu_register_sitting_callback(sitting_event_callback_t callback);

/**
 * @brief 检查是否在睡眠时段
 * @return true 睡眠时段，false 非睡眠时段
 */

bool imu_is_sleep_time(void);

/**
 * @brief 重置久坐状态
 */

void imu_reset_sitting_state(void);

/**
 * @brief 获取静止持续时间
 * @return 静止秒数
 */

uint32_t imu_get_still_duration(void);

#endif /* __APP_SILVER_GUARDIAN_BAND_INCLUDE_IMU_H */
