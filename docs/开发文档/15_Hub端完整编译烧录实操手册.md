# 15 Hub 端完整编译·打包·烧录·上板 实操手册

> 面向要复现/协作的开发者。按顺序做即可从源码到板子显示「银发守护」界面。全部命令均已在 Gemini-S1 板子验证通过。

---

## 一、下载源码（环境准备）

```bash
# 1. 进入 repo 工作区（Windows 侧在 D:\Desktop\首届openvela比赛\contest2026_325_junweiyanjiuyuan）
wsl -d Ubuntu-24.04
cd ~/contest2026_325_junweiyanjiuyuan

# 2. 确认 repo 工作区完整（若有缺失项目）
repo sync -c -j8
# ⚠️ 报 tricore/aurix/sil-kit 等 notdefault 项目报错可忽略

# 3. 关键：确认应用的编译源是"嵌套"目录！
#    openvela 通过 linkfile 编译 packages/demos/contest2026_325_silver_guardian_hub
#    它软链指向嵌套 contest2026_325_junweiyanjiuyuan/app/silver_guardian_hub
ls -la packages/demos/contest2026_325_silver_guardian_hub
#    改应用源码要改这里指向的嵌套目录，改完必须 touch 强制重编
```

---

## 二、编译

```bash
cd ~/contest2026_325_junweiyanjiuyuan
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/ -j8
# 产物：nuttx/vela.bin（内核+应用）
```

**判断是否真更新**（重要，别信 uname 时间戳——它编译时写死不变）：
```bash
ls -l nuttx/vela.bin                      # 时间应为刚刚
strings nuttx/vela.bin | grep "Initializing LCD"   # 应能看到 LCD 字符串
```

**改源码后要强制重编**（否则 mtime 旧被增量跳过）：
```bash
touch <编译源>/src/main.c <编译源>/src/lcd.c
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/ -j8
```

---

## 三、打包成可烧录镜像

> ⚠️ `build.sh` 只产出 `vela.bin`，**必须 `pack` + dragon 才有 PhoenixSuit 能烧的 `.img`**。

### 1) pack
```bash
cd ~/contest2026_325_junweiyanjiuyuan/vendor/allwinnertech/lichee
source envsetup.sh
lunch_nuttx r528s3-gemini-s1
pack
```

### 2) 替换出厂 fes1/boot0（**不改 PhoenixSuit 卡 0%**）
PhoenixSuit 只认出厂固件的 fes1/boot0。
```bash
# 从出厂固件提取（首次）：
#   用 python 从 gemini_s1_mini.img 提取 fes1.fex@0xba400(21504B)、boot0_nand.fex@0x12c00(45056B)
cd out/r528s3/gemini-s1_nand/image
# 用出厂 fes1/boot0 覆盖编译生成的（我保存在本机 tools/）：
python3 - <<EOF
import glob,shutil
for f in ["fes1.fex","boot0_nand.fex"]:
    src=glob.glob("/mnt/d/Desktop/*/tools/"+f)[0]
    shutil.copy(src,f); print(f,"done")
EOF
```

### 3) dragon 重新打包（带第二参数，否则只 2MB）
```bash
cd <lichee>
source envsetup.sh >/dev/null 2>&1; lunch_nuttx r528s3-gemini-s1 >/dev/null 2>&1
cd out/r528s3/gemini-s1_nand/image
~/contest2026_325_junweiyanjiuyuan/vendor/allwinnertech/lichee/tools/tool/dragon image.cfg sys_partition_for_dragon.fex
# 产物：out/r528s3/gemini-s1_nand/image/rtos_nuttx_r528s3-gemini-s1_uart0_128Mnand.img
```

### 4) 拷贝到英文路径
```bash
cp <out>/image/rtos_nuttx...img /mnt/c/Users/Harryminei/flash_lcd.img
cp <out>/image/rtos_nuttx...img "/mnt/d/Desktop/首届openvela比赛/tools/flash_lcd.img"
```

---

## 四、烧录（PhoenixSuit）

1. 打开 `D:\Desktop\首届openvela比赛\tools\PhoenixSuit_new\PhoenixSuit.exe`
2. 点 **「一键刷机」** → 浏览选 `C:\Users\Harryminei\flash_lcd.img`（英文路径）
3. 模式选 **「全盘擦除升级」**
4. 点 **「刷机」** → 程序进入"等待设备"
5. **板子断电 3~5 秒再上电**（物理触发 FEL，比 adb reboot 更可靠）
6. 进度到 **100%** 后板子自动重启

> 若卡 0%：回第三步检查 fes1/boot0 是否出厂版；若提示"固件文件打开失败"：bootloader 分区 size 应=16384；若工具异常：**重新解压 PhoenixSuit**。

---

## 五、上板验证

```bash
adb devices                      # 应见 1234 device
adb shell uname -a               # 固件版本（时间戳写死，别用来判断新旧）
adb shell dmesg | grep -i "LCD system initialized"   # 关键：LCD 接管成功
adb shell ps | grep silver       # 应用是否自启
```

**期望日志**：
```
[lcd] Initializing LCD system
lcd /dev/lcd0 open success
[lcd] LCD system initialized (screen 320x240)
[silver_hub] LCD system initialized
```

---

## 六、常见问题速查
| 现象 | 原因 / 处理 |
|---|---|
| 界面一直灰屏/旧画面 | vela.bin 没更新或应用没编进（查第2层根因）；确认用嵌套编译源 + 重新 pack |
| 中文乱码只显示"守" | `lv_font_simsun_16_cjk` 示例字体 Unicode 覆盖不全，需换全常用汉字字库（task） |
| 无法触摸 | `/dev/input0` 绑了但 indev 输入层/坐标未生效，需调 LVGL indev（task） |
| `multiple definition of lcd_init` | 应用 `lcd_init` 与系统 `panels.c` 冲突，改名 `silver_lcd_init` |
| 编译改了没进固件 | `touch` 源码强制重编；`ls vel.bin` 确认更新 |
