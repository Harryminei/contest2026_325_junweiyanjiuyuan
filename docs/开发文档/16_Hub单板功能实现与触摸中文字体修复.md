# 16 · Hub 端单板功能实现与触摸/中文字体修复

> 本文对应 2026-09-12 的工作。目标：让 Gemini-S1 **单独一块板子**就能交互演示——
> 中文正常显示、触摸可用、用药/健康/设置等功能全在板上闭环，不依赖手环和云端。
>
> 每一步都给了可复制的命令，照着敲即可复现。

---

## 一、这次做了什么

| 项 | 改前 | 改后 |
|---|---|---|
| 中文显示 | 用 LVGL 自带示例字库（仅 1166 字形），"银发守护"只出"守" | 自生成 GB2312 全字库（3755 汉字 + ASCII + 标点），16px/24px 两档 |
| 触摸 | `/dev/input0` 能打开但点击无反应 | 独立诊断探针 + 触摸自检页，先定位再修；无触摸时自动演示模式兜底 |
| 界面 | 只有静态 label，**没有任何可点控件** | 9 个页面，全部按钮/开关/列表可触摸操作 |
| 用电管理 | 硬编码一条假计划，重启即丢 | 可增删改、可调时间剂量、落盘到 `/data`，重启不丢 |
| 音频 | 打开 `/dev/i2s0`（**该节点不存在**），且送的是全 0 假数据 | 修正为 `/dev/audio/pcm0p`，代码生成 PCM 提示音，失败可观测 |
| 传感器 | 完全没有 | 读板上 SHTC3 / LTR553 / SGP30 真实数据 |
| 云端 | 100% 空壳但对外假装成功 | 保留接口、如实标注为本地桩，不再假报成功 |

---

## 二、必须先理解的两件事

### 2.1 源码有两份，编译用的不是 git 跟踪的那份

openvela 用 repo manifest 的 `<linkfile>` 把比赛仓库里的 app 目录映射到编译位置
（`contest2026_325_junweiyanjiuyuan.xml`）：

```xml
<linkfile src="app/silver_guardian_hub"
          dest="packages/demos/contest2026_325_silver_guardian_hub"/>
```

于是同一份代码存在三处：

| | 路径 | 说明 |
|---|---|---|
| **A** 开发源 | `~/contest2026_325_junweiyanjiuyuan/app/silver_guardian_hub` | 被 git 跟踪，提交推送到这里 |
| **B** 编译源 | `~/contest2026_325_junweiyanjiuyuan/contest2026_325_junweiyanjiuyuan/app/silver_guardian_hub` | linkfile 目标，**make 真正读的那份** |
| **C** Windows 副本 | `D:\Desktop\首届openvela比赛\contest2026_325_junweiyanjiuyuan\app\silver_guardian_hub` | 本地留存/查阅 |

**A 和 B 不会自动同步。** 2026-09-06 那次"改了界面代码却编不进固件"就是栽在这里。
本仓库提供 `tools/sync_app.sh` 专门管这件事，**编译前务必先 push**。

### 2.2 固件分区只有 8MB，字库不能随便加

`vela.bin` 打包成 `nsh.fex`，放进 `sys_partition.fex` 的 `bootloader` 分区：

```
[partition]
     name         = bootloader
     size         = 16384        ; 扇区 -> 16384 x 512 = 8 MB
     downloadfile = "nsh.fex"
```

改之前 `vela.bin` 已经 **7.31MB**，只剩 **715KB** 余量。
所以加字库的同时必须把 LVGL 自带的那份示例 CJK 字库关掉（省约 230KB），
24px 标题字库也压到 1bpp。编译脚本每次都会检查最终大小。

---

## 三、一键流程（日常就用这四条）

在 WSL 里，进入工作区：

```bash
cd ~/contest2026_325_junweiyanjiuyuan
```

```bash
# 1) 同步：开发源 -> 编译源 + Windows 副本
bash tools/sync_app.sh push

# 2) 编译 + 自动校验 vela.bin 真的更新了
bash tools/build_hub.sh

# 3) 打包成可烧录镜像（别直接跑 pack，见下面的坑）
bash tools/pack_hub.sh

# 4) Windows 侧用 PhoenixSuit 烧录 C:\Users\Harryminei\flash_sg.img
```

### ⚠️ 第 3 步为什么不能直接 `pack`

**`pack` 会重新生成 `fes1.fex` / `boot0_nand.fex`，而 PhoenixSuit 只认出厂固件里那一份。
用了编译生成的，烧录会卡在 0% 不动。** 实测差异：

| 文件 | 出厂版（能烧） | `pack` 生成的（卡 0%） |
|---|---|---|
| `fes1.fex` | 21504 B `d63e3a84…` | 20640 B `a9c93f88…` |
| `boot0_nand.fex` | 45056 B `13c5c05a…` | 45056 B `6a455123…` |

`tools/pack_hub.sh` 把这套流程封好了：pack → 换回出厂 fes1/boot0 → **校验 md5** →
用 dragon 重新打包（第二个参数 `sys_partition_for_dragon.fex` 不能省，否则产物只有 2MB）
→ 拷到纯英文路径 `C:\Users\Harryminei\flash_sg.img`。

出厂版 fes1/boot0 存在 `D:\Desktop\首届openvela比赛\tools\` 下，**不在 git 里**。
丢了的话从出厂固件提取：`fes1.fex` @ `0xba400` 长 21504、`boot0_nand.fex` @ `0x12c00` 长 45056。

**烧录时选英文路径的镜像** —— PhoenixSuit 读带中文的路径容易出问题。

想先看看三份源码是否一致：

```bash
bash tools/sync_app.sh status
```

---

## 四、中文字体修复

### 4.1 根因

原来用的 `lv_font_simsun_16_cjk` 是 **LVGL 自带的示例字库**，它的 CJK cmap 只有
1166 个字形（`range_start=12527, list_length=1166`），"银""发""护"都不在里面，
所以屏幕上只剩"守"字。这不是配置问题，是字库本身覆盖不全。

### 4.2 字体选型（注意授权）

**不要用 SimHei / SimSun**——那是中易授权给微软的商业字体，嵌进 Apache-2.0 的
开源仓库有授权风险。

用 **Noto Sans SC**（思源黑体的 Google 版），授权 **SIL OFL 1.1**，
明确允许嵌入、修改、再分发，包括生成位图字体。而且黑体笔画粗细均匀，
16px 位图下比宋体清晰得多。

### 4.3 生成字库

```bash
# 一次性装工具（约 3MB，装在 ~/lvfont，不进仓库）
mkdir -p ~/lvfont && cd ~/lvfont && npm install lv_font_conv

cd ~/contest2026_325_junweiyanjiuyuan
bash tools/gen_lvgl_fonts.sh          # 自动下载字体并生成两档字库
```

产物：`app/silver_guardian_hub/src/fonts/sg_font_16.c` / `sg_font_24.c`

| 文件 | 字号 | bpp | 位图 | 用途 | 选型理由 |
|---|---|---|---|---|---|
| `sg_font_16` | 16 | 4 | 446 KB | 正文/列表/按钮 | 小字号必须抗锯齿，汉字笔画才分得开 |
| `sg_font_24` | 24 | 1 | 224 KB | 页面标题 | 大字号 1bpp 已够清晰，省一半空间 |

字符集由 `tools/gen_font_charset.py` 生成：GB2312 一级汉字 3755 个 + ASCII + 中文标点，
共 3956 字符。想加减字符改这个脚本再重跑。

### 4.4 代码里怎么用

`include/fonts.h` 里做了语义化别名，换字号只改这一处：

```c
LV_FONT_DECLARE(sg_font_16);
LV_FONT_DECLARE(sg_font_24);

#define SG_FONT_TEXT    (&sg_font_16)             /* 正文 */
#define SG_FONT_TITLE   (&sg_font_24)             /* 标题 */
#define SG_FONT_CLOCK   (&lv_font_montserrat_48)  /* 大时钟数字 */
#define SG_FONT_SMALL   (&lv_font_montserrat_16)  /* 纯数字小字 */
```

### 4.5 关掉没用的自带字库（必须做）

板级 defconfig 在 vendor 树里、**不在比赛仓库**，repo sync 后会被重置，
所以用一个幂等脚本管：

```bash
bash tools/patch_board_config.sh          # 执行
bash tools/patch_board_config.sh --check  # 只检查
```

它做一件事：把 `CONFIG_LV_FONT_SIMSUN_16_CJK` 关掉。
（`build.sh` 每次会用 defconfig 重新生成 `.config`，所以直接编译即生效。）

---

## 五、触摸修复

### 5.1 先搞清楚链路两端是什么

```
GT911 电容触摸（I2C0）
  └─ vendor/allwinnertech/boards/r528/drivers/gt911_iic_touch.c
       └─ touch_register("/dev/input0")     <- r528_boot.c 里注册
            └─ LVGL: lv_nuttx_touchscreen_create("/dev/input0")
                    （CONFIG_LV_USE_NUTTX_TOUCHSCREEN=y）
```

**从 2026-09-06 的串口日志能确认 GT911 是存在的**：

```
lv_nuttx_touchscreen_create: touchscreen /dev/input0 open success
```

这行只有在 **open 成功 且 `TSIOC_GETMAXPOINTS` ioctl 成功且非 0** 时才打印。
而 `/dev/input0` 只可能由 `gt911_register()` 创建，它能走到 `touch_register()`
的前提是 GT911 已经在 I2C 上被识别到。**所以"芯片不在板上"这个假设可以排除**，
问题在事件通路。

### 5.2 还有一个容易被忽略的可能

改之前的界面**全是 `lv_label`，没有任何可点控件**。在那种界面上，
"点了没反应"本来就是必然结果，跟触摸好不好使无关。
所以这次先把界面换成真按钮，再谈触摸有没有问题。

### 5.3 这次加的诊断手段

**a) 独立触摸探针**（`src/touch.c`）

绕开 LVGL 直接读 `/dev/input0`，统计样本数、按下/移动/抬起次数、坐标范围、
flags 原始值。NuttX 触摸上层是把事件广播给**所有**打开的 fd，
所以这个探针和 LVGL 的 indev 互不干扰。

**b) 触摸自检页**（菜单 → 触摸自检）

不用串口，屏幕上直接看：

- `样本 / 按下 / 移动 / 抬起 / 空` 计数
- `maxpoint`、最后一次 `x y flags`、以及观测到的 **坐标范围** `x[min,max] y[min,max]`
- 下方一块蓝色区域，手指滑动时红点跟着走

**坐标范围**是关键：手指划过整屏后，如果 `x` 只落在很窄的区间，说明 GT911
的分辨率配置和屏幕不匹配，需要做坐标缩放。

**c) 原始事件打日志**

开机后的前 20 个触摸样本会逐条打到 syslog，串口直接能看：

```
[touch] #1 n=1 flags=0x01 x=123 y=45
```

### 5.4 三种结果怎么处理

| 自检页看到的现象 | 结论 | 下一步 |
|---|---|---|
| 计数在涨、红点跟着走、按钮有按下反馈 | 触摸本来就是好的，之前是界面没有可点控件 | 无需改动 |
| 计数在涨，但坐标范围很窄 / 明显偏 | GT911 分辨率与屏幕不符 | 在 `lv_nuttx_touchscreen` 侧或在应用里做坐标缩放（改动点明确） |
| 计数一直是 0 | 驱动层没产生事件 | 串口看有无 `GT911 ... failed`；检查 `TOUCH_POINT_GET_LARGE` 位是否被误判（该位在部分 GT911 固件上正常触摸也会置位） |

### 5.5 兜底：演示模式

万一触摸确实不可用，界面也不能变成砖头：

- LVGL 的 indev 建不起来 → 开机直接进演示模式
- 或者 **30 秒内没有任何用户输入** → 自动进演示模式，页面每 6 秒轮播一次
- 底部状态栏显示"演示模式 · 触摸屏幕退出"，任意触摸即退出
- 也可以在功能菜单里手动开关"演示模式"

---

## 六、音频（这次唯一没法离线验证的部分）

### 6.1 根因

旧代码打开的是 `/dev/i2s0` —— **这个节点根本不存在**。

实际配置：

```
defconfig:  CONFIG_AUDIO_CUSTOM_DEV_PATH=y
.config:    CONFIG_AUDIO_DEV_PATH="/dev/audio"
r528_boot.c: audio_register("pcm0p", sunxi_audio_initialize(false, 0));
```

所以播放节点是 **`/dev/audio/pcm0p`**。

### 6.2 新实现

`src/audio.c` 按 `apps/system/nxplayer/nxplayer.c` 的顺序走：

```
open(/dev/audio/pcm0p)
  -> AUDIOIOC_CONFIGURE       设定 PCM / 16bit / 单声道 / 16kHz
  -> AUDIOIOC_SETBUFFERINFO   按数据量申请管线缓冲
  -> AUDIOIOC_ALLOCBUFFER     分配缓冲
  -> 填 PCM -> AUDIOIOC_ENQUEUEBUFFER
  -> AUDIOIOC_START -> 等待 -> AUDIOIOC_STOP
  -> AUDIOIOC_FREEBUFFER
```

**提示音由代码生成**（正弦波 + 首尾 5ms 淡入淡出防爆音），不需要任何音频文件：

| 提示音 | 用途 |
|---|---|
| `TONE_CLICK` | 按键反馈 |
| `TONE_STARTUP` | 开机上行三音 |
| `TONE_MEDICATION` | 用药提醒"叮咚" |
| `TONE_SITTING` | 久坐提醒 |
| `TONE_SOS` | 紧急呼救急促报警 |
| `TONE_ERROR` | 警告 |

播放放在独立线程里，SOS 提示音 1 秒多也不会卡住 LVGL 主循环。

### 6.3 出不了声时怎么查

**先绕开应用代码，用板子自带的 nxplayer 验证硬件通路**（`CONFIG_SYSTEM_NXPLAYER=y`）。
串口进 NSH：

```
nsh> ls /dev/audio
nsh> nxplayer
nxplayer> device pcm0p
nxplayer> play /data/test.wav
```

如果 `nxplayer` 也放不出声，那问题在 codec/I2S 配置层，跟应用无关。
如果 `nxplayer` 能出声而应用不能，看应用日志里 `[audio]` 打在哪一步失败。

### 6.4 关于 TTS

TTS 需要联网，单板离线做不到。旧实现是"调 `cloud_tts()` 拿一段全 0 的假 buffer
再写设备"，结果是必然没声音还假装成功。现在 `audio_play(text)` 如实降级为
播放提示音 + 一条日志说明，不再假装。

---

## 七、板上传感器

三颗传感器都在 i2c2 上，由 `r528_boot.c` 启动时通过 NuttX uORB 框架注册：

| 传感器 | 测量 | 设备节点 |
|---|---|---|
| SHTC3 | 温度 | `/dev/uorb/sensor_temp0` |
| SHTC3 | 湿度 | `/dev/uorb/sensor_humi0` |
| LTR553 | 光照 | `/dev/uorb/sensor_light0` |
| LTR553 | 接近 | `/dev/uorb/sensor_prox0` |
| SGP30 | CO2 | `/dev/uorb/sensor_co20` |
| SGP30 | TVOC | `/dev/uorb/sensor_tvoc0` |

`src/sensors.c` 以非阻塞方式读，1 秒刷新一次，读不到就沿用上次的值。
**每一路都允许打开失败**——驱动注册前会做 `checkid`，芯片不在就注册失败
（`r528_boot.c` 忽略返回值），节点不存在。页面上显示"未检测到"而不是崩掉。

自检办法：菜单 → 关于，看 `传感器: N/6`。或者串口：

```
nsh> ls /dev/uorb
```

---

## 八、持久化

`rcS.nsh` 里有一句 `mount -t yaffs /dev/usrdata /data`，
defconfig 也开了 `CONFIG_FS_YAFFS`，所以 **`/data` 是掉电不丢的可写分区**。

`src/storage.c` 在其下建 `/data/silver_guardian/`，写文件用
"临时文件 + rename" 两步，避免掉电留半截文件。

- 用药计划 → `medication.dat`
- 设置 → `settings.dat`

持久化不可用时（比如分区挂载失败）所有接口退化成"只在内存里成功"，
不会让上层崩掉，但"关于"页会显示 `存储: 不可用`。

---

## 九、界面结构

```
┌────────────────────────────────┐  0
│ [返回] 页面标题       联网状态 │  32px 状态栏（lcd.c 维护）
├────────────────────────────────┤  32
│         页面内容区 320x176     │  每个页面是它的子对象
├────────────────────────────────┤  208
│      守护状态 / 演示模式提示   │  32px 底栏
└────────────────────────────────┘  240
```

警示层（SOS/久坐/用药）挂在 `lv_layer_top()` 上，**不管当前在哪一页都能弹出来**，
非 SOS 的 15 秒自动收起。

功能菜单 8 个入口：

```
用药提醒   健康数据
事件记录   设置
触摸自检   关于
久坐提醒   演示模式      <- 后两个是单板演示/自测用的手动触发
```

---

## 十、上板验证清单

烧录后按这个顺序走一遍，每步该看到什么写清楚了：

1. **开机** — 屏上出现"银发守护"标题 + 大时钟 + 日期，**中文不乱码**
2. **菜单** — 点「功能菜单」，8 个按钮都能点亮（按下变蓝）
3. **触摸自检** — 菜单 → 触摸自检
   - 看 `样本` 计数是否随手指增长
   - 在蓝色区域滑动，红点是否跟着走
   - 划遍整屏后看 `范围 x[min,max] y[min,max]` 是否覆盖 0–319 / 0–175
4. **用药提醒** — 点一条计划进编辑页，改时间/剂量、切启用停用、返回看列表是否同步
   - 点「立即测试提醒」→ 应弹蓝色用药警示层 + 响提示音
   - **断电重启**，看计划是否还在（验证持久化）
5. **健康数据** — 看温湿度/光照等是否有真实读数；手靠近板子看接近值变化
6. **事件记录** — 刚才触发的用药/久坐事件有没有记进来
7. **设置** — 调音量后点「保存并应用」，重启看是否保留
8. **关于** — 看自检行：`触摸 / 音频 / 传感器 N/6 / 存储`
9. **音频** — 任何一个提示音有没有真的响
10. **SOS** — 主页点「紧急求助」，应弹红色警示层 + 急促报警音，
    点「知道了」才收起（SOS 不自动收）

---

## 十一、排障速查

| 现象 | 先看哪里 |
|---|---|
| 界面上还是旧行为 | 忘了 `sync_app.sh push`。跑 `sync_app.sh status` 看 A/B 是否一致 |
| **改了代码、编译也成功、`strings vela.bin` 能查到新字符串，但板子行为就是不变** | **`out/.../image/nsh.fex` 是旧的**（dragon 读它，但 build.sh 只写 `board/.../configs/nsh.fex`，两者不同步）。用 `pack_hub.sh`（已内置强制同步与校验），别直接 `pack` |
| **改了代码但 vela.bin 大小和上一版一模一样** | 两种可能：① 改动确实小于 4KB，vela.bin 按 4KB 对齐，属正常；② 改动被 `push(A→C)` 覆盖了 —— 见下一条 |
| **Windows 侧改的代码"消失"** | `sync_app.sh push` 是 A→C，会覆盖 C 上更新的文件。`build_hub.sh` 现在是 `pull(C→A) → gen_fonts → push(A→B,C) → build`，顺序不能换 |
| 编译"成功"但屏上没变化 | 看 `build_hub.sh` 输出的 `vela.bin` 时间戳和 strings 校验；链接失败时 vela.bin 不更新，但日志时间戳照刷 |
| 链接报 `multiple definition` | 符号和系统框架重名。已知 `lcd_init` 与 R528 显示框架冲突，应用侧已改名 `silver_lcd_init` |
| `nsh.fex` 超过 8MB | 分区塞不下。看 `ls -l .../out/r528s3/gemini-s1_nand/image/nsh.fex`，超了就要压字库 bpp 或精简字符集 |
| 中文还是缺字 | 字库字符集里没有那个字。字符集会**自动扫描源码**收录用到的汉字，改完源码重跑 `gen_lvgl_fonts.sh` 即可 |
| **中文变成一个个方框，数字正常** | 该控件用了 ASCII-only 字体。`SG_FONT_ASCII_ONLY`(=Montserrat) 没有中文字形，中文一律用 `SG_FONT_TEXT` |
| 改了 defconfig 没生效 | repo sync 会把 vendor 树重置掉，重跑 `tools/patch_board_config.sh` |
| 找不到音频设备 | 串口 `ls /dev/audio` 看实际节点名，改 `audio.c` 里的 `g_dev_candidates[]` |
| **传感器读不到** | 先 `adb shell ls /dev/uorb` 看实际节点名。SHTC3 温度是 `sensor_ambient_temp0`，**不是** `sensor_temp0` |
| adb 认不到板子 | PhoenixSuit 占着 adb。先 `Stop-Process adb` 再 `adb start-server` |
| **按复位键后黑屏** | 复位原因若为 `restore` 会执行 `/etc/factory.sh`（**强制格式化 `/data`**）。重启请用断电上电，别按复位键 |
| 烧录卡 0% | fes1/boot0 不是出厂版。用 `pack_hub.sh` |
| 烧完行为没变，怀疑烧错文件 | 读 `adb shell cat /data/silver_guardian/diag.txt` 首部的**构建标记**（纯 ASCII，如 `SGHUB-BUILD-20260912-1300`）确认板上是哪一版 |

### 板上诊断速查（adb 直连）

```bash
adb shell cat /data/silver_guardian/diag.txt   # 各模块真实状态，首选
adb shell ls /dev/uorb                          # 传感器实际节点名
adb shell ls /dev/audio                         # 音频实际节点名
adb shell ps | grep silver                      # 应用是否在跑
adb shell resetcause                            # 上次复位原因
```

`diag.txt` 开机写一次、之后每 30 秒刷新。**注意它每次都会重写 NAND**，
长期挂机演示建议把 `main.c` 里的 `DIAG_INTERVAL_MS` 调大或改为按需写。

---

## 十二、文件清单

### 新增

| 文件 | 作用 |
|---|---|
| `app/silver_guardian_hub/src/ui.c` | 9 个页面与导航 |
| `app/silver_guardian_hub/src/touch.c` `include/touch.h` | 触摸诊断探针 |
| `app/silver_guardian_hub/src/sensors.c` `include/sensors.h` | 板上 uORB 传感器 |
| `app/silver_guardian_hub/src/storage.c` `include/storage.h` | `/data` 持久化 |
| `app/silver_guardian_hub/include/fonts.h` | 字体声明与语义别名 |
| `app/silver_guardian_hub/src/fonts/*.c` | 生成的 GB2312 位图字库 |
| `tools/sync_app.sh` | 三处源码同步 |
| `tools/build_hub.sh` | 一键编译 + vela.bin 更新校验 |
| `tools/gen_lvgl_fonts.sh` | 生成中文字库 |
| `tools/gen_font_charset.py` | 生成字符集 |
| `tools/patch_board_config.sh` | 修补 vendor 树里的板级 defconfig |

### 重写

| 文件 | 主要变化 |
|---|---|
| `src/lcd.c` | 从"静态显示"改成显示底座：状态栏 + 内容区 + 警示层 + 演示模式 |
| `src/audio.c` | 修正设备节点，改为 NuttX audio 框架 + 代码生成提示音 + 播放线程 |
| `src/medication.c` | 修掉漏服判定与跨天重置的缺陷，加落盘与编辑接口 |
| `src/event.c` | 加事件历史环形缓冲，处理器改用新的警示层 API |
| `src/main.c` | 新的初始化顺序，各模块失败不致命 |
| `include/lcd.h` `ui.h` `audio.h` `medication.h` `event.h` | 跟随更新 |

---

## 十三、和手环联通的预留

单板这套跑通后，接第二块板子（手环）只需要往事件系统里发事件：

```c
event_trigger_sos();                    /* 手环按 SOS */
event_trigger_sitting(3600);            /* 手环 IMU 判定久坐，参数是秒 */
event_trigger_medication("降压药");     /* 用药确认来自手环 */
```

事件系统会自动完成"提示音 + 警示层 + 事件记录"这一整套动作。
BLE/WiFi 接收端只需要调这三个函数，界面侧不用改。
