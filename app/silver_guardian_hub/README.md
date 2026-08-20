# Silver Guardian Hub - Hub端应用

## 概述

银发守护系统Hub端应用，运行在润芯微 Gemini-S1 平台上。

## 核心功能

1. **事件处理系统**
   - 事件队列管理
   - 优先级处理
   - 事件分发和回调

2. **音频系统**
   - TTS 语音合成（云端）
   - 音频播放
   - 录音功能

3. **用药提醒**
   - 多种药物管理
   - 定时提醒
   - 多次提醒机制
   - 服药记录

4. **云端通信**
   - WiFi 连接管理
   - REST API 调用
   - ASR/TTS 服务
   - 告警上报
   - 健康数据同步

## 编译

```bash
# 进入 openvela 工作区根目录
cd ~/contest2026_325_junweiyanjiuyuan/..

# 编译
./build.sh vendor/openvela/boards/gemini_s1/configs/nsh -j8
```

## 烧录

使用 USB 或 SD 卡烧录到 Gemini-S1 开发板。

## 运行

```bash
# 在 NuttX Shell 中
nsh> silver_guardian_hub
```

## 目录结构

```
silver_guardian_hub/
├── src/
│   ├── main.c          # 主程序
│   ├── event.c         # 事件处理
│   ├── audio.c         # 音频播放
│   ├── medication.c    # 用药提醒
│   └── cloud.c         # 云端通信
├── include/
│   ├── event.h
│   ├── audio.h
│   ├── medication.h
│   └── cloud.h
├── configs/
├── Kconfig             # 配置选项
├── Makefile            # 构建脚本
└── README.md
```

## 配置选项

通过 `menuconfig` 配置：

- 最大用药计划数
- 云端 API 地址
- 默认音量
- 任务优先级
- 堆栈大小

## 事件处理流程

```
手环端 BLE 告警
    ↓
事件队列
    ↓
事件处理器
    ↓
├─ SOS → 语音播报 + 云端告警
├─ 久坐 → 语音提醒 + 云端记录
├─ 用药 → LCD显示 + 语音播报
└─ 云端命令 → 执行对应操作
```

## 开发计划

- [x] 基础框架
- [x] 事件处理系统
- [x] 音频播放框架
- [x] 用药提醒功能
- [x] 云端通信框架
- [ ] 完善 TTS 集成
- [ ] 完善 ASR 集成
- [ ] 完善 LLM 对话
- [ ] LCD 界面开发
- [ ] 功耗优化
