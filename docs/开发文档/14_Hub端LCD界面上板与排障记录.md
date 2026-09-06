# 14 Hub 端 LCD 界面上板与排障记录（2026-09-06）

> **成果**：Hub 端 `silver_guardian_hub` 应用（含 LCD/LVGL 界面）完成编译→打包→烧录→上板，**LCD 界面成功显示**（`LCD system initialized (screen 320x240)`），标题/时钟/日期/用药弹窗均出现，完整功能（Event/Audio/Medication/Cloud/TTS）运行。

---

## 一、三层根因链（本次攻关核心，务必牢记）

今天界面一直上不了屏，不是界面代码的问题，而是**三层连环坑**，任何一层没排查出来，改多少界面代码都无效：

### 第 1 层：开发源码与编译源脱节
- openvela 用 repo manifest 构建应用：
  ```xml
  <linkfile src="app/silver_guardian_hub" dest="packages/demos/contest2026_325_silver_guardian_hub"/>
  ```
  软链指向的是**嵌套目录** `contest2026_325_junweiyanjiuyuan/app/silver_guardian_hub`（旧版，无 LCD）。
- 而开发（git 历史里的 LCD 代码）改在**根目录** `app/silver_guardian_hub`（新版，有 LCD）。
- 结果：编译永远用的是嵌套旧版，LCD 代码从未被编译。
- **排查**：`ls -la packages/demos/contest2026_325_silver_guardian_hub` 看软链指向；`find . -path "*silver_guardian_hub*main.c"` 会发现多个副本。
- **修复**：把根 app 源码同步到嵌套编译源。

### 第 2 层：`lcd_init` 符号冲突（致命）
- R528 芯片显示框架 `vendor/allwinnertech/chips/r528/.../disp2/disp/lcd/panels.c` **已经定义了 `lcd_init`**。
- 我们应用也叫 `lcd_init` → 链接报错：
  ```
  ld: multiple definition of `lcd_init';  panels.o (symbol from plugin): first defined here
  ```
- **修复**：应用 `lcd_init` 改名 `silver_lcd_init`（定义/声明/调用三处），避开系统符号。

### 第 3 层：`vela.bin` 不更新
- 链接失败（第 2 层）导致 `nuttx/vela.bin` 一直停在旧版（应用无 LCD），后续每次 `pack` 用的都是旧 vela.bin → 固件永远无 LCD。
- **判断固件是否更新，别只看 `uname` 时间戳**（编译时写死，每次都是 `18:10:33`，**不会变**）。要看：
  ```bash
  ls -l nuttx/vela.bin                                  # 时间是否变新
  strings nuttx/vela.bin | grep "Initializing LCD"       # 是否含新代码
  ```
- **修复**：改名消除冲突后链接成功，`rm nuttx/vela.bin` 重新 `objcopy -O binary nuttx vela.bin`，确认 strings 含 LCD 字符串再打包。

---

## 二、避坑经验
| 坑 | 解决方法 |
|---|---|
| 改的源码没生效 | 编译需确认用的是**编译源(嵌套)**，而非 dev 根；改后 `touch` 强制重编（否则 mtime 旧被增量跳过） |
| `uname` 判断固件 | 不可靠，时间戳写死；用 `vela.bin` 时间 + strings 确认 |
| `lcd_init` 重复定义 | 与系统 `panels.c` 冲突，应用改名避开 |
| 编译源目录 | `packages/demos/...` 软链指向嵌套 app，改这里才生效 |

---

## 三、本次上板验证结果
✅ 编译成功（`multiple definition`=0，main.c/lcd.c 均重编）
✅ `vela.bin` 含 LCD 代码（`Initializing LCD system` / `LCD system initialized (screen 320x240)`）
✅ 上板 dmesg：
```
[lcd] Initializing LCD system
lv_nuttx_lcd_create: lcd /dev/lcd0 open success
lv_nuttx_touchscreen_create: touchscreen /dev/input0 open success
[lcd] LCD system initialized (screen 320x240)
[silver_hub] LCD system initialized
```

⚠️ **待修（两个界面细节）**：
1. **中文乱码**：`FONT_CJK16 = &lv_font_simsun_16_cjk`（LVGL 内置示例 CJK 字体），Unicode 覆盖不全 → "银发守护"只渲染出"守"。需换覆盖全常用汉字的 CJK 字库（思源黑体 / 全量 GB2312）。
2. **无法触摸**：`/dev/input0` 绑定 success 但点击无交互 → LVGL indev 输入事件 / 坐标转换需调。

---

## 四、固件产物
- 本地：`D:\Desktop\首届openvela比赛\tools\flash_20260906_lcd.img`（可烧，含 LCD）
- WSL 工作区：`~/contest2026_325_junweiyanjiuyuan/flash_20260906_lcd.img`
- LICENSEH build：`vendor/allwinnertech/lichee/out/r528s3/gemini-s1_nand/image/rtos_nuttx...img`
- git commit：`020d1a7`（lcd_init→silver_lcd_init 修复）
