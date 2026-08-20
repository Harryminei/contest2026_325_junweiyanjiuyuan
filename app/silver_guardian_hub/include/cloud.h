/****************************************************************************
 * Silver Guardian Hub - 云端通信模块头文件
 ****************************************************************************/

#ifndef __APP_SILVER_GUARDIAN_HUB_INCLUDE_CLOUD_H
#define __APP_SILVER_GUARDIAN_HUB_INCLUDE_CLOUD_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdbool.h>
#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define CLOUD_API_URL_MAX_LEN   256
#define CLOUD_TOKEN_MAX_LEN     128
#define CLOUD_RESPONSE_MAX_LEN  1024

/* 告警类型 */

#define ALERT_TYPE_SOS          "sos"
#define ALERT_TYPE_FALL         "fall"
#define ALERT_TYPE_SITTING      "sitting_reminder"
#define ALERT_TYPE_MEDICATION   "medication_missed"
#define ALERT_TYPE_ABNORMAL     "abnormal"

/* 告警等级 */

#define ALERT_LEVEL_LOW         "low"
#define ALERT_LEVEL_MEDIUM      "medium"
#define ALERT_LEVEL_HIGH        "high"
#define ALERT_LEVEL_CRITICAL    "critical"

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* 音频缓冲区 */

typedef struct
{
  uint8_t *data;
  uint32_t size;
} audio_buffer_t;

/* 云端状态 */

typedef struct
{
  bool wifi_connected;
  bool api_connected;
  int8_t wifi_signal;
  uint32_t last_sync_time;
} cloud_status_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * @brief 初始化云端通信
 * @return 0 成功
 */

int cloud_init(void);

/**
 * @brief 反初始化云端通信
 */

void cloud_deinit(void);

/**
 * @brief 处理云端通信（主循环调用）
 */

void cloud_process(void);

/**
 * @brief TTS 语音合成
 * @param text 要合成的文本
 * @return 音频缓冲区，需手动释放
 */

audio_buffer_t *cloud_tts(const char *text);

/**
 * @brief 释放音频缓冲区
 * @param buffer 音频缓冲区
 */

void free_audio_buffer(audio_buffer_t *buffer);

/**
 * @brief ASR 语音识别
 * @param audio_data 音频数据
 * @param size 数据大小
 * @return 识别文本，需手动释放
 */

char *cloud_asr(const uint8_t *audio_data, uint32_t size);

/**
 * @brief LLM 对话
 * @param message 用户消息
 * @return AI 回复，需手动释放
 */

char *cloud_llm_chat(const char *message);

/**
 * @brief 上报告警
 * @param alert_type 告警类型
 * @param message 告警消息
 * @return 0 成功
 */

int cloud_report_alert(const char *alert_type, const char *message);

/**
 * @brief 上报健康数据
 * @param type 数据类型
 * @param value 数据值（JSON 格式）
 * @return 0 成功
 */

int cloud_report_health(const char *type, const char *value);

/**
 * @brief 上报活动状态
 * @param status 状态描述
 * @return 0 成功
 */

int cloud_report_activity(const char *status);

/**
 * @brief 记录对话
 * @param user_msg 用户消息
 * @param ai_reply AI 回复
 * @return 0 成功
 */

int cloud_save_conversation(const char *user_msg,
                            const char *ai_reply);

/**
 * @brief 获取云端状态
 * @return 状态结构体指针
 */

const cloud_status_t *cloud_get_status(void);

/**
 * @brief 检查 WiFi 连接
 * @return true 已连接
 */

bool cloud_is_wifi_connected(void);

/**
 * @brief 同步时间
 * @return 0 成功
 */

int cloud_sync_time(void);

/**
 * @brief 获取用户配置
 * @return 0 成功
 */

int cloud_fetch_config(void);

#endif /* __APP_SILVER_GUARDIAN_HUB_INCLUDE_CLOUD_H */
