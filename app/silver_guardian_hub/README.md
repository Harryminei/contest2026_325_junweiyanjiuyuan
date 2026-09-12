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

5. **LCD 界面（LVGL）**
   - 主界面：时钟、日期、网络/守护状态
   - 警示界面：SOS(红) / 久坐(橙) / 用药(蓝)

## 编译

```bash
# 进入 openvela 工作区根目录
cd ~/contest2026_325_junweiyanjiuyuan/..

# 编译（2.8寸 SPI 屏配置，已验证 ✅）
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/ -j8
```

> ⚠️ 若要界面显示中文，需在板级 `nsh_minidisplay/defconfig` 中加 `CONFIG_LV_FONT_SIMSUN_16_CJK=y`（LVGL 默认字体不含汉字）。

## 烧录

使用 PhoenixSuit 烧录打包产物 `lichee/out/r528s3/gemini-s1_nand/rtos_nuttx_r528s3-gemini-s1_uart0_128Mnand.img`。
注意：需使用出厂固件配套的 fes1/boot0 引导文件（否则 PhoenixSuit 无法烧录）。

## 运行

```bash
# 在 NuttX Shell 中
nsh> silver_guardian_hub

# 查看运行日志
nsh> dmesg
```

## 已验证（2026-08-24）

- 编译打包烧录全链路打通
- 板子运行自编译 openvela 系统
- 应用初始化成功：事件/音频/用药提醒/云端，欢迎语音 TTS 触发
- 待办：I2S 音频驱动配置（当前语音播报无声）

## 目录结构

```
silver_guardian_hub/
├── src/
│   ├── main.c          # 主程序
│   ├── event.c         # 事件处理
│   ├── audio.c         # 音频播放
│   ├── medication.c    # 用药提醒
│   ├── cloud.c         # 云端通信
│   └── lcd.c           # LCD/LVGL 界面
├── include/
│   ├── event.h
│   ├── audio.h
│   ├── medication.h
│   ├── cloud.h
│   └── lcd.h           # LCD 界面接口
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
- [x] LCD 界面开发（LVGL）
- [ ] 功耗优化
