# 更新日志

所有重要更改都会记录在此文件中。

格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.0.0/)。

## [未发布]

## [0.4.0] - 2026-08-30

### 新增 - Hub端 LCD 界面（LVGL）
- 新增 `src/lcd.c` / `include/lcd.h` LVGL 界面模块
  - 通过 `lv_nuttx_init` 绑定 `/dev/lcd0`（2.8寸 ILI9341 SPI 屏）
  - 主界面：时钟（48号大字）、日期、顶部状态栏（标题+联网状态）、底部守护状态
  - 警示界面：SOS(红) / 久坐(橙) / 用药(蓝)，事件触发弹出，10秒自动返回主界面
- `event.c`：SOS / 久坐 / 用药 事件 → 调用 LCD 警示界面（替换原 TODO）
- `main.c`：集成 `lcd_init / lcd_task / lcd_deinit`，主循环 20ms 驱动 LVGL，并同步网络状态
- `Makefile`：加入 `src/lcd.c`；`CMakeLists.txt`：加 `DEPENDS lvgl`；`Kconfig`：补充 LCD 说明
- 启用中文字体 `CONFIG_LV_FONT_SIMSUN_16_CJK`（板级 defconfig，LVGL 默认字体无汉字）
- 新增详细上板操作手册 `docs/开发文档/12_Hub端上板操作手册.md`

## [0.3.0] - 2026-08-24

### 重大进展 - Hub端编译打包烧录上板验证全链路打通
- Hub端应用 `silver_guardian_hub` 成功编译进 openvela 固件
- 适配 openvela packages 构建系统（CMakeLists.txt / Makefile / Make.defs / Kconfig）
- manifest 添加 `silver_guardian_hub` 的 linkfile 集成到构建树
- 编译 `nsh_minidisplay` 配置通过，打包固件成功
- 修复 update_mbr 分区关联 bug（script 工具合并 partition 导致 nsh.fex 错配 sst 分区）
- 修复 PhoenixSuit 2.0.0 烧录兼容问题（bootloader 分区 16384 / 使用出厂 fes1+boot0）
- **板子烧录成功并运行自编译系统**，hub 应用初始化成功：
  - 事件系统 ✅
  - 音频系统 ✅（I2S 暂未配置）
  - 用药提醒 ✅（降压药计划）
  - 云端通信 ✅
  - 欢迎语音 TTS 已触发

### 其他
- 记录 AI Coding 日志到 `logs/harryminei/2026-08-24/`
- 更新 README 构建说明、项目结构、开发计划

## [0.2.1] - 2026-08-21

### 优化
- 更新文档中的时间线
- 同步本地仓库

## [0.2.0] - 2026-08-20

### 新增
- 项目文档整理完成（21份技术文档）
- 开发操作手册
- Apache License 2.0 开源协议

### 优化
- 更新 README.md 项目说明
- 统一文档结构

## [0.1.0] - 2026-08-15 ~ 2026-08-19

### 新增 - 手环端（2026-08-15 ~ 2026-08-17）
- SOS 一键呼救功能（2026-08-16）
  - 长按按键触发
  - BLE 通知 Hub
  - 取消机制
- IMU 久坐检测（2026-08-16）
  - 六轴 IMU 数据采集
  - 运动状态识别
  - 60分钟久坐提醒
  - 2小时静止告警
- BLE 通信框架（2026-08-16）
  - GATT 服务
  - 数据上报
- LVGL UI 界面（2026-08-17）
  - 主界面
  - SOS 界面
  - 久坐提醒界面

### 新增 - Hub端（2026-08-18 ~ 2026-08-19）
- 事件处理系统（2026-08-18）
  - 事件队列
  - 优先级处理
- 音频播放框架（2026-08-18）
  - TTS 播放
  - 音量控制
- 用药提醒功能（2026-08-18）
  - 多药物管理
  - 定时提醒
  - 服药记录
- 云端通信框架（2026-08-19）
  - WiFi 连接
  - REST API
  - 告警上报

### 新增 - 开发环境（2026-08-14 ~ 2026-08-15）
- openvela 开发环境配置
- 开发工具安装
- AI Coding 日志系统配置

## [0.0.1] - 2026-08-14

### 新增
- 项目初始化
- 仓库创建
- 基础目录结构

