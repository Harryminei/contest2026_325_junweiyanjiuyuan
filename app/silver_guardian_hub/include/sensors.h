/****************************************************************************
 * Silver Guardian Hub - 板上传感器
 *
 * Gemini-S1 板载三颗 I2C 传感器，都挂在 i2c2 上，由 r528_boot.c 在启动时
 * 通过 NuttX uORB 框架注册：
 *   SHTC3  温湿度      /dev/uorb/sensor_temp0  sensor_humi0
 *   LTR553 光照/接近   /dev/uorb/sensor_light0 sensor_prox0
 *   SGP30  空气质量    /dev/uorb/sensor_co20   sensor_tvoc0
 *
 * 注意：这些驱动都会先做 checkid，芯片不在就注册失败（r528_boot.c 忽略返回值），
 * 设备节点就不存在。所以每个通道都必须能容忍"打开失败"，页面上显示"未检测到"，
 * 而不是让整个应用起不来。
 ****************************************************************************/

#ifndef __APP_SILVER_GUARDIAN_HUB_INCLUDE_SENSORS_H
#define __APP_SILVER_GUARDIAN_HUB_INCLUDE_SENSORS_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdbool.h>
#include <stdint.h>

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* 一个传感器通道 */

typedef struct
{
  const char *name;      /* 中文显示名 */
  const char *unit;      /* 单位 */
  const char *path;      /* uORB 设备节点 */
  int         fd;        /* -1 表示不可用 */
  bool        opened;    /* 节点是否打开成功 */
  bool        valid;     /* 是否读到过有效值 */
  float       value;     /* 最新值 */
  int         err;       /* 打开失败时的 errno */
} sg_sensor_t;

/* 传感器通道索引 */

enum
{
  SG_SENSOR_TEMP = 0,    /* 温度 °C */
  SG_SENSOR_HUMI,        /* 湿度 % */
  SG_SENSOR_LIGHT,       /* 光照 lux */
  SG_SENSOR_PROX,        /* 接近 cm */
  SG_SENSOR_CO2,         /* CO2 ppm */
  SG_SENSOR_TVOC,        /* TVOC ppm */
  SG_SENSOR_COUNT
};

typedef struct
{
  sg_sensor_t ch[SG_SENSOR_COUNT];
  int         opened_count;   /* 成功打开的通道数 */
  uint32_t    last_update_ms;
  uint32_t    update_count;
} sensors_state_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * @brief 打开所有传感器节点（失败只记录，不致命）
 * @return 成功打开的通道数（0 表示一个都没有）
 */

int sensors_init(void);

/**
 * @brief 关闭所有节点
 */

void sensors_deinit(void);

/**
 * @brief 按 1Hz 拉取数据（主循环调用；非阻塞，读不到就保留上次值）
 * @param now_ms 当前毫秒时间
 */

void sensors_poll(uint32_t now_ms);

/**
 * @brief 取传感器状态快照
 */

const sensors_state_t *sensors_get(void);

/**
 * @brief 某个通道是否可用（节点已打开）
 */

bool sensors_is_present(int index);

#endif /* __APP_SILVER_GUARDIAN_HUB_INCLUDE_SENSORS_H */
