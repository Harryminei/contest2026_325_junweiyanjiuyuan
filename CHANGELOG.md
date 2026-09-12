# 更新日志

所有重要更改都会记录在此文件中。

格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.0.0/)。

## [未发布]

## [1.1.0] - 2026-09-12

### 重大进展 - Hub 端单板可交互（触摸界面 + 中文全字库 + 本地功能闭环）

**单板不再依赖手环和云端**：Gemini-S1 单独一块板子即可完成触摸交互、
用药提醒、健康数据、事件记录、设置与自检。

#### 修复
- **中文缺字**：`lv_font_simsun_16_cjk` 是 LVGL 示例字库，CJK 部分仅 1166 个字形，
  "银发守护"只有"守"能显示。改为自生成 GB2312 全字库（3755 汉字 + ASCII + 标点）
  - 源字体 Noto Sans SC（SIL OFL 1.1，可嵌入可再分发；**不用** SimHei/SimSun 等商业字体）
  - `sg_font_16`（16px/4bpp，正文）+ `sg_font_24`（24px/1bpp，标题）
- **触摸无响应**：新增独立诊断探针与"触摸自检"页，先定位再修
  - 由 09-06 串口日志 `touchscreen /dev/input0 open success`（该行仅在 open + ioctl
    都成功时才打印）确认 GT911 物理存在且 I2C 正常，问题在事件通路
  - 另发现改前界面**全是 label、没有任何可点控件**，在那样的界面上"点了没反应"本属必然
- **音频无声**：`audio.c` 打开的 `/dev/i2s0` 节点根本不存在。实际播放节点是
  `/dev/audio/pcm0p`（`CONFIG_AUDIO_DEV_PATH="/dev/audio"` + `audio_register("pcm0p",...)`）
- **用药漏服判定失效**：旧逻辑要求主循环恰好在计划那一分钟内被调度到才触发提醒，
  漏服上报又塞在该分支内，实际几乎不可能执行；跨天重置用 `hour==0 && minute==0` 同理
  - 改为按天记账（`last_fired_day` 日期戳），到点触发、每 5 分钟重提醒最多 3 次、
    次数用尽再过 30 分钟判漏服上报

#### 新增 - 触摸驱动的多页界面
- 新增 `src/ui.c`：9 个页面，全部触摸可操作
  主页 / 功能菜单 / 用药提醒 / 编辑用药 / 健康数据 / 事件记录 / 设置 / 关于 / 触摸自检
- `src/lcd.c` 从"静态显示"重构为显示底座：状态栏 + 内容区 + 底栏，
  警示层挂在 `lv_layer_top()` 上，任意页面都能弹出
- 无触摸兜底：LVGL indev 建不起来、或 30 秒无输入 → 自动进入演示模式轮播页面

#### 新增 - 板上传感器（真实数据）
- 新增 `src/sensors.c`，读 NuttX uORB 框架下的三颗 I2C 传感器：
  SHTC3（温度/湿度）、LTR553（光照/接近）、SGP30（CO2/TVOC）
- 每路都容忍打开失败（驱动注册前会 `checkid`，芯片不在即注册失败），页面显示"未检测到"

#### 新增 - 本地持久化
- 新增 `src/storage.c`，落盘到 `/data/silver_guardian/`
  （`rcS.nsh` 里 `mount -t yaffs /dev/usrdata /data`，掉电不丢）
- 写文件用"临时文件 + rename"两步防掉电半截文件
- 用药计划与设置重启后保留

#### 新增 - 本地提示音
- 提示音由代码生成 PCM 正弦波（含首尾 5ms 淡入淡出防爆音），不依赖任何音频文件
- 6 种提示音：按键反馈 / 开机 / 用药 / 久坐 / SOS 报警 / 警告
- 播放走独立线程，SOS 报警 1 秒多也不阻塞 LVGL 主循环

#### 新增 - 构建工具链
- `tools/sync_app.sh`：三处源码同步（git源 ↔ 编译源 ↔ Windows 副本）
- `tools/build_hub.sh`：一键编译 + `vela.bin` 更新校验（防"链接失败但日志时间戳照刷"的假成功）
- `tools/gen_lvgl_fonts.sh` + `tools/gen_font_charset.py`：生成中文字库
- `tools/patch_board_config.sh`：修补 vendor 树里的板级 defconfig（repo sync 会重置）

#### 变更 - 如实标注未实现的部分
- `cloud.c` 仍是本地桩（无任何 socket 代码）。去掉"假成功"：
  `audio_play(text)` 离线无 TTS 时如实降级为提示音并记日志，
  `medication_sync_from_cloud()` 返回 `-ENOSYS` 而不是假装成功

#### 约束
- `vela.bin` 打包进 `sys_partition.fex` 的 `bootloader` 分区（16384 扇区 = 8MB）
- 当前 `nsh.fex` = 7.87MB，**余量仅 135KB**。再加功能需先压缩字库
  （`SG_SIZES_16=16:3 bash tools/gen_lvgl_fonts.sh` 可省约 114KB）

#### 文档
- 新增 `docs/开发文档/16_Hub单板功能实现与触摸中文字体修复.md`（含完整复现步骤与排障速查）
- 固件：`firmware/flash_20260912_touch_font.img`

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

