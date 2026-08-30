# 12 Hub端全功能上板操作手册

**文档版本**: v1.1
**更新时间**: 2026-08-30
**适用项目**: 银发守护 - 手环+桌面Hub双端AI守护系统（Hub 端，润芯微 Gemini-S1）
**说明**: 本文档针对「已有一块 Gemini-S1 板子，需将 Hub 端全功能跑通并在 2.8 寸 LCD 上显示」的完整操作流程，覆盖编译 → 打包 → 烧录 → 运行 → LCD 验证。

---

## 📌 一、功能概览（本次新增 LCD 界面后）

Hub 端应用 `silver_guardian_hub` 现已具备全部核心功能，并**全部在 LCD 屏上可视**：

| 功能 | 说明 | LCD 呈现 |
|------|------|----------|
| 事件处理系统 | 事件队列 + 优先级分发 | 事件触发时弹出警示界面 |
| 用药提醒 | 多药物定时提醒、多次提醒、服药记录 | 蓝色「用药提醒」界面 |
| 久坐/静止提醒 | 由手环端 IMU 触发 | 橙色「久坐提醒」界面 |
| SOS 紧急呼救 | 手环端按键触发 | 红色「紧急求助！」界面 |
| 云端通信 | WiFi 连接、告警上报、TTS/ASR/LLM | 顶部状态栏显示联网状态 |
| **LCD 界面（本次新增）** | LVGL 主界面 + 事件警示界面 | 时钟、日期、守护状态、网络状态 |

**主界面**（默认）：
- 顶部状态栏：左侧「银发守护」标题，右侧「已联网」/「未联网」
- 中部：大号时钟 `HH:MM`（48 号数字字体）、日期（X月X日 星期X）
- 底部：守护状态提示（如「守护中」）

**警示界面**（事件触发自动弹出，10 秒后自动返回主界面）：
- SOS：深红底 + 「紧急求助！/ 已发出 / 正在通知家属...」
- 久坐：橙底 + 「您已静坐 X 分钟 / 请起身活动一下」
- 用药：蓝底 + 「请服用 药品名」

---

## 🗂️ 二、代码结构（本次新增 lcd 模块）

```
app/silver_guardian_hub/
├── src/
│   ├── main.c          # 主程序（集成 lcd_init/lcd_task/lcd_deinit）
│   ├── event.c         # 事件处理（SOS/久坐/用药 触发 LCD 警示）
│   ├── audio.c         # 音频播放（TTS）
│   ├── medication.c    # 用药提醒
│   ├── cloud.c         # 云端通信
│   └── lcd.c           # ★ 新增：LVGL 界面模块
├── include/
│   ├── event.h
│   ├── audio.h
│   ├── medication.h
│   ├── cloud.h
│   └── lcd.h           # ★ 新增：LCD 界面接口
├── Kconfig             # 已更新（LCD 功能说明）
├── Makefile            # 已加入 src/lcd.c
├── CMakeLists.txt      # 已加入 DEPENDS lvgl
└── README.md
```

`lcd.c` 关键实现：
- 用 `lv_nuttx_dsc_init()` + `lv_nuttx_init()` 绑定 `/dev/lcd0`（2.8 寸 ILI9341 SPI 屏）
- 用 LVGL 中文字体 `lv_font_simsun_16_cjk` 显示中文
- 主界面 + 警示界面（复用一屏，切换背景色与文案）

---

## 🔧 三、编译

### 3.1 前置检查

1. 进入 WSL2：`wsl -d Ubuntu-24.04`
2. 进入工作区：
   ```bash
   cd ~/contest2026_325_junweiyanjiuyuan
   ```
3. 确认代码为最新（含本次 lcd 模块）：
   ```bash
   git pull origin dev-ai-contest-2026   # 或确保本地已同步
   ```

### 3.2 开启界面中文字体（很重要！）

LVGL 默认 Montserrat 字体**不含汉字**，不开启会显示方块。为让 LCD 显示中文，需在板级配置中启用宋体：

**编辑文件**（WSL 路径）：
```
vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/defconfig
```
**在文件末尾追加一行**：
```
CONFIG_LV_FONT_SIMSUN_16_CJK=y
```
> `CONFIG_LV_FONT_SIMSUN_16_CJK` 对应 LVGL 内置宋体 16px 中文字体（位于 `apps/graphics/lvgl/lvgl/src/font/lv_font_simsun_16_cjk.c`，Kconfig 定义在 `lvgl/lvgl/Kconfig`）。本次任务已把它加到板级配置，若 `repo sync` 覆盖了请重新加上。

### 3.3 编译固件

```bash
cd ~/contest2026_325_junweiyanjiuyuan

# 若此前配置过且改过 Kconfig/defconfig，先彻底清理避免残留 .config
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/ distclean

# 编译（2.8 寸 SPI 屏配置）
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/ -j8
```
- 编译完成后应用会出现在构建树中（`CONFIG_LVX_USE_DEMO_CONTEST2026_325_SILVER_GUARDIAN_HUB=y`）。
- 若报某些项目缺失，先 `repo sync -c -j8`（注意 `tricore/aurix/sil-kit` 等是无关项目，可忽略其报错）。
- 若首次 configure 报 `-mthumb/-mfloat-abi` 错误，说明残留了残缺 `.config`，务必先 `distclean` 再编译。

### 3.4 编译产物确认

应用可执行文件应出现在 NuttX 二进制构建目录中，并已打包进固件。

---

## 📦 四、打包固件

```bash
cd ~/contest2026_325_junweiyanjiuyuan/vendor/allwinnertech/lichee
source envsetup.sh
lunch_nuttx r528s3-gemini-s1
pack
```

打包产物路径：
```
vendor/allwinnertech/lichee/out/r528s3/gemini-s1_nand/rtos_nuttx_r528s3-gemini-s1_uart0_128Mnand.img
```

> 打包时若 `update_mbr` 报 `dl file nsh.fex size too large`，是本机已修复的分区关联 bug 被 `repo sync` 覆盖所致，需重新替换修复后的 `update_mbr`（见仓库 `docs/开发文档/` 排障记录）。

---

## 🔥 五、烧录

### 5.1 首选：PhoenixSuit USB 烧录（已验证 ✅）

**原理**：通过 USB 让板子进入 FEL 下载模式，把 `.img` 烧入板载 NAND。

**电脑端准备**：
- PhoenixSuit V2.0.0：`D:\Desktop\首届openvela比赛\tools\PhoenixSuit\PhoenixSuit.exe`
- 固件：按「四、打包」生成的 `.img`（或使用参考目录 `/tools/flash_v5.img`）
- 驱动：全志 USB 驱动需已安装（FEL 模式 VID_1F3A&PID_EFE8）。若未装，用管理员安装 `PhoenixSuit\Drivers\AW_Driver\InstallUSBDrv.exe` 或运行 `python tools/install_driver.py`

**操作步骤**：
1. 运行 `PhoenixSuit.exe` →「一键刷机」界面
2. 浏览选择固件 `.img`，模式选「**全盘擦除升级**」
3. 点「刷机」→ 按提示让板子进入 FEL（可用 adb 触发重启，或板子断电再上电瞬间进 FEL）
4. PhoenixSuit 弹出确认 → 等待进度条完成，烧录完成后板子自动启动

**⚠️ 关键要点（易踩坑）**：
- **必须用出厂固件配套的 `fes1.fex` 和 `boot0_nand.fex` 作为引导**，不能用我们编译的。出厂参考 `tools/gemini_s1_mini.img`。
- 打包后需将出厂 `fes1/boot0` 替换到打包 `image/` 目录，再手动 `dragon image.cfg sys_partition_for_dragon.fex`（必须带第二参数，否则 img 只有 2MB）。
- `update_mbr`/分区表问题会导致 PhoenixSuit 卡 0% 或报「固件文件打开失败」，按分区表排障（bootloader 分区 size=16384）。
- 固件路径用英文（如 `C:\Users\Public\firmware\`），别用中文路径。

### 5.2 关于 SD 卡 / 读卡器方式（说明）

板级配置已启用 MMCSD/SDIO（`CONFIG_MMCSD=y`、`CONFIG_DRIVERS_SDMMC=y`），因此 **Gemini-S1 支持挂载 microSD/TF 卡作为存储**（如存用药记录、日志）。但注意区分两点：

| 用途 | 状态 | 说明 |
|------|------|------|
| **TF 卡作为存储**（存文件/数据） | 驱动已使能 ✅ | 需要配置 FAT/存储文件系统并手动 mount（如 `mount /dev/mmcsd0 /mnt`），请先确认板上卡槽引脚与驱动匹配 |
| **从 SD 卡启动/烧录固件** | 未验证 ⚠️ | 全志 R528 BROM 理论上支持卡启动，但 NAND 版板子默认从 NAND 引导；卡启动需要专门的分区/镜像布局，**建议先用 PhoenixSuit 烧 NAND 验证功能**，卡启动作为后续加分项 |

> 结论：**先走 PhoenixSuit 烧 NAND（已验证、最稳）**。TF 卡数据存储可在功能跑通后再接，若需要我可以帮你配置挂载与写入。

---

## ▶️ 六、运行与验证

### 6.1 运行应用

板子启动后，在 NuttX Shell（NSH / 串口 / adb）中：

```bash
# 打印内核日志（确认启动）
dmesg

# 运行 Hub 应用
silver_guardian_hub
```

### 6.2 预期日志

```
[  ...] [silver_hub] Silver Guardian Hub starting...
[  ...] [silver_hub] Version: 1.0.0
[  ...] [silver_hub] Event system initialized
[  ...] [silver_hub] Audio system initialized
[  ...] [silver_hub] Medication system initialized
[  ...] [silver_hub] Cloud system initialized
[  ...] [lcd] LCD system initialized (screen 320x240)
[  ...] [silver_hub] Hub system initialized successfully
[  ...] [silver_hub] Starting Hub main loop
```
- **关键一行**：`[lcd] LCD system initialized (screen 320x240)` —— 表示 LVGL 已成功绑定显示。
- 若 LCD 行缺失或报 `LVGL display attach failed`，说明显示绑定失败（检查 `/dev/lcd0` 与 LVGL/说明配置）。

### 6.3 LCD 界面验证清单（逐项打钩 ✅）

| # | 检查项 | 预期现象 |
|---|--------|----------|
| 1 | 开机后运行应用 | 屏幕显示主界面：顶栏「银发守护」+ 联网状态 |
| 2 | 大时钟 | 中央显示当前时间并每分钟跳动 |
| 3 | 日期 | 显示「X月X日 星期X」 |
| 4 | 中文显示 | 「银发守护」「已联网」等为中文（不是方块）。若是方块 → 中文字体未开启（回 3.2） |
| 5 | 用药提醒 | 到点（默认降压药 08:00）弹出蓝色「用药提醒」，10 秒后自动回主界面 |
| 6 | SOS 弹出（可手动触发） | 手环或事件触发后弹红色「紧急求助！」，语音播报 |
| 7 | 久坐提醒 | 手环久坐事件触发后弹橙色「您已静坐 X 分钟」 |
| 8 | 返回主界面 | 警示结束后回到主界面，状态栏刷新 |

> 注：用药默认计划为「降压药 08:00」，可用 `menuconfig` 或代码调整；若要立即触发测试须改计划时间或临时在 `medication_process` 中改时间。

---

## 📟 七、日志与排障

- **串口/usb 日志**：USB-TTL 接 UART2（PC0/PC1），波特率 **1500000**（1.5M），用 mobaxterm；或 `adb logcat` / `adb shell dmesg`。
- **adb**：板子 APP Mode 时 `adb devices` 应为 `1234`。
- **查看应用日志**：运行应用后 `dmesg`。
- **常见问题**：
  - 屏幕黑屏/无显示：确认 `/dev/lcd0` 存在、LVGL 已初始化、中文字体已开。
  - 中文方块：中文字体未开启（3.2 节）。
  - 无法烧录：按 5.1 的⚠️排查（fes1/boot0、分区表、驱动、英文路径）。
  - 应用崩溃：检查堆栈大小（`STACKSIZE`，现 32768）是否足够（LVGL 需要较大栈）。

---

## ✍️ 八、本次变更汇总

- 新增 `src/lcd.c`、`include/lcd.h`（LVGL 界面）
- `event.c`：SOS/久坐/用药事件 → 调用 LCD 警示界面
- `main.c`：集成 `lcd_init/lcd_task/lcd_deinit`，主循环 20ms 驱动 LVGL
- `Makefile`/`CMakeLists.txt`：加入 `src/lcd.c`、`DEPENDS lvgl`
- `Kconfig`：补充 LCD 功能说明与中文字体提示
- 板级 `defconfig`：新增 `CONFIG_LV_FONT_SIMSUN_16_CJK=y`（中文字体）
- `README.md`、`CHANGELOG.md`：同步更新
