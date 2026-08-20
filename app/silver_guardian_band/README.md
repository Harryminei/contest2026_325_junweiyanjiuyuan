# Silver Guardian Band - 手环端应用

## 概述

银发守护系统手环端应用，运行在黄山派 SF32LB52 平台上。

## 核心功能

1. **SOS 一键呼救**
   - 长按 SOS 按键 3 秒触发
   - BLE 通知 Hub 端
   - 10 秒内可取消

2. **久坐检测**
   - 六轴 IMU 数据采集
   - 端侧运动状态识别
   - 60 分钟久坐提醒
   - 2 小时静止告警

3. **BLE 通信**
   - GATT 服务
   - SOS 告警上报
   - 活动状态同步

4. **LVGL 界面**
   - 大字体显示
   - SOS 界面
   - 久坐提醒界面
   - 主界面（时间、步数、电量）

## 编译

```bash
# 进入 openvela 工作区根目录
cd ~/contest2026_325_junweiyanjiuyuan/..

# 编译
./build.sh vendor/openvela/boards/sf32lb52/configs/nsh -j8
```

## 烧录

使用 J-Link 或串口烧录到 SF32LB52 开发板。

## 运行

```bash
# 在 NuttX Shell 中
nsh> silver_guardian_band
```

## 目录结构

```
silver_guardian_band/
├── src/
│   ├── main.c      # 主程序
│   ├── sos.c       # SOS 呼救
│   ├── imu.c       # IMU 和久坐检测
│   ├── ble.c       # BLE 通信
│   └── ui.c        # LVGL 界面
├── include/
│   ├── sos.h
│   ├── imu.h
│   ├── ble.h
│   └── ui.h
├── configs/
├── Kconfig         # 配置选项
├── Makefile        # 构建脚本
└── README.md
```

## 配置选项

通过 `menuconfig` 配置：

- SOS 按键引脚
- SOS 长按时间
- 久坐提醒阈值
- 静止告警阈值
- IMU 采样率
- BLE 开关
- 任务优先级
- 堆栈大小

## 开发计划

- [x] 基础框架
- [x] SOS 呼救功能
- [x] IMU 驱动和久坐检测
- [x] BLE 通信框架
- [x] LVGL 界面框架
- [ ] 完善 IMU 算法
- [ ] 完善 BLE 数据传输
- [ ] 完善 UI 界面美化
- [ ] 功耗优化
