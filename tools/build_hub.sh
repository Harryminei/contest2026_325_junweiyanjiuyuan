#!/usr/bin/env bash
#
# 银发守护 Hub —— 一键编译 + 更新校验
#
# 完整流程（顺序不能换，理由见下方大字注释）：
#   pull(C->A) -> gen_fonts -> push(A->B,C) -> build -> 校验
#
# 它挡掉的几个坑（都实际踩过）：
#   1. 改了代码没同步到编译源 -> 编出来还是旧行为
#   2. 先 push 再改 -> Windows 侧的新代码被 A 的旧版本覆盖掉，
#      现象是"编译成功但 vela.bin 大小与上一版完全一样"
#   3. 链接失败时 vela.bin 不更新，但日志时间戳照旧刷新 -> 假成功
#   4. 固件超出 8MB bootloader 分区 -> pack 成功但烧录卡住
#
# 用法（WSL，仓库根目录）：
#   bash tools/build_hub.sh              # 全流程
#   bash tools/build_hub.sh --no-sync    # 跳过收发改动（只在 A 里改过时用）
#   bash tools/build_hub.sh --no-fonts   # 跳过字库生成（改字库参数时才用）
#   JOBS=16 bash tools/build_hub.sh      # 指定并发（默认 8）
#
# 编完之后用 tools/pack_hub.sh 打包成可烧 .img（别直接跑 pack，会卡 0%）
#
set -uo pipefail

WORKSPACE="${HOME}/contest2026_325_junweiyanjiuyuan"
BOARD_CONFIG="vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/"
JOBS="${JOBS:-8}"
VELA="${WORKSPACE}/nuttx/vela.bin"
LOG="${WORKSPACE}/build_hub_$(date +%Y%m%d_%H%M%S).log"

DO_SYNC=1
DO_FONTS=1
for arg in "$@"; do
  case "$arg" in
    --no-sync)  DO_SYNC=0 ;;
    --no-fonts) DO_FONTS=0 ;;
    -h|--help) sed -n '2,22p' "$0"; exit 0 ;;
    *) echo "未知参数: $arg" >&2; exit 2 ;;
  esac
done

cd "$WORKSPACE" || { echo "工作区不存在: $WORKSPACE" >&2; exit 1; }

# ============================================================ 顺序很重要
#
#   pull(C->A)  ->  gen_fonts(A)  ->  push(A->B,C)  ->  build
#
# 每一步的位置都不能换，都是踩过坑换来的：
#
#   * pull 必须在最前
#     代码通常在 Windows 侧（C）编辑，而 push 是 A->C，先 push 会把 C 上
#     更新的源码用 A 的旧版本盖掉。实测丢过一次 audio.c 的全部改动，
#     现象是"编译成功但 vela.bin 大小和上一版一模一样"。
#
#   * gen_fonts 必须在 pull 之后、push 之前
#     字库是生成物，pull 已排除 src/fonts/*.c（见 sync_app.sh），所以不会被
#     覆盖；紧接着 push 会把它同步到编译源和 Windows 副本。
#     反过来先 gen 再 pull，新字库会被 Windows 侧的旧字库覆盖，
#     结果生成日志是新字形数、编进固件的是旧的，缺字照旧。

if [ "$DO_SYNC" -eq 1 ]; then
  echo "########## [0/5] 收回 Windows 侧的改动 (C -> A) ##########"
  bash tools/sync_app.sh pull | grep -E '^>f|^cd|LF 化' | head -20
  echo ""
else
  echo "########## [0/5] 跳过收回改动 (--no-sync) ##########"
  echo "警告：Windows 侧的改动不会被收进来，可能编的是旧代码。"
  echo ""
fi

if [ "$DO_FONTS" -eq 1 ]; then
  echo "########## [1/5] 重新生成中文字库 ##########"
  if bash tools/gen_lvgl_fonts.sh 2>&1 | tail -4; then
    echo ""
  else
    echo "字库生成失败（不影响继续，但字库可能不是最新的）" >&2
    echo ""
  fi
else
  echo "########## [1/5] 跳过字库生成 (--no-fonts) ##########"
  echo ""
fi

if [ "$DO_SYNC" -eq 1 ]; then
  echo "########## [2/5] 同步开发源 -> 编译源 + Windows 副本 ##########"
  bash tools/sync_app.sh push | grep -E '^>f|^cd' | head -20
  echo ""
else
  echo "########## [2/5] 跳过同步 (--no-sync) ##########"
  echo ""
fi

# ------------------------------------------------- 强制重编：绕开头文件依赖缺失
#
# 这个 app 没有 src/Make.dep，make 根本不跟踪 .h -> .c 的依赖。
# 只改 .h 不改 .c 的话改动**永远进不了固件**：实测改了 include/diag.h，
# diag.h 的 mtime 明明比 diag.c.o 新，make 依然不重编 diag.c，构建标记
# 就一直停在旧值（和"vela.bin 不更新"是同一族坑）。
#
# push 用的是 rsync -a，mtime 原样带过来，所以只能在 push 之后显式 touch。
# app 只有十来个 .c，全量重编十几秒，换"改哪都一定生效"。

APP_B="${WORKSPACE}/contest2026_325_junweiyanjiuyuan/app/silver_guardian_hub"

if [ -d "$APP_B" ]; then
  find "$APP_B" -name '*.c' -exec touch {} +
  echo "########## [2.5/5] 已 touch app 全部 .c（强制重编，绕开头文件依赖缺失） ##########"
  echo ""
fi

# ------------------------------------------------------- 2. 记录编译前的基线
BEFORE_MTIME="(不存在)"
BEFORE_SIZE=0
if [ -f "$VELA" ]; then
  BEFORE_MTIME="$(date -r "$VELA" '+%Y-%m-%d %H:%M:%S')"
  BEFORE_SIZE="$(stat -c %s "$VELA")"
fi
echo "########## [3/5] 编译前 vela.bin 基线 ##########"
echo "  时间: $BEFORE_MTIME"
echo "  大小: $BEFORE_SIZE bytes"
echo ""

# ------------------------------------------------------------------ 3. 编译
echo "########## [4/5] 编译 (config=${BOARD_CONFIG}, -j${JOBS}) ##########"
echo "  完整日志: $LOG"
echo ""
./build.sh "$BOARD_CONFIG" -j"$JOBS" 2>&1 | tee "$LOG"
BUILD_RC="${PIPESTATUS[0]}"
echo ""

if [ "$BUILD_RC" -ne 0 ]; then
  echo "########## 编译失败 (exit=$BUILD_RC) ##########" >&2
  echo "先看错误，常见原因：" >&2
  echo "  - multiple definition / undefined reference -> 符号与系统框架冲突或漏加源文件" >&2
  echo "  - 找不到头文件 -> Makefile 的 CSRCS/CFLAGS 没更新" >&2
  grep -nE 'error:|Error [0-9]|undefined reference|multiple definition' "$LOG" | tail -20 >&2
  exit "$BUILD_RC"
fi

# ------------------------------------------------------------- 4. 更新校验
echo "########## [5/5] vela.bin 更新校验 ##########"
if [ ! -f "$VELA" ]; then
  echo "  失败：vela.bin 不存在！链接大概率失败了。" >&2
  exit 1
fi

AFTER_MTIME="$(date -r "$VELA" '+%Y-%m-%d %H:%M:%S')"
AFTER_SIZE="$(stat -c %s "$VELA")"
echo "  编译前: $BEFORE_MTIME  (${BEFORE_SIZE} bytes)"
echo "  编译后: $AFTER_MTIME  (${AFTER_SIZE} bytes)"

# 分区容量硬校验：vela.bin 会打包成 nsh.fex 放进 sys_partition.fex 的
# bootloader 分区（16384 扇区 x 512 = 8MB）。超了就烧不进去，
# 而且现象是"pack 成功、烧录卡住"，很难查，所以在编译阶段就拦下来。
PART_LIMIT=8388608
echo "  分区上限: ${PART_LIMIT} bytes (bootloader, 16384 扇区 x 512)"
if [ "$AFTER_SIZE" -gt "$PART_LIMIT" ]; then
  echo "" >&2
  echo "  !! 固件超出分区 $((AFTER_SIZE - PART_LIMIT)) 字节，烧不进去！" >&2
  echo "     压缩办法（按收益排序）：" >&2
  echo "       1) 24px 字库降到 1bpp：SG_SIZES_24=24:1 bash tools/gen_lvgl_fonts.sh  省约 270KB" >&2
  echo "       2) 16px 字库降到 3bpp：SG_SIZES_16=16:3 bash tools/gen_lvgl_fonts.sh  省约 110KB" >&2
  echo "       3) 精简 tools/gen_font_charset.py 的字符集" >&2
  exit 1
fi
echo "  余量    : $((PART_LIMIT - AFTER_SIZE)) bytes"

if [ "$BEFORE_MTIME" = "$AFTER_MTIME" ] && [ "$BEFORE_SIZE" = "$AFTER_SIZE" ]; then
  echo ""
  echo "  !! vela.bin 没有变化。两种可能：" >&2
  echo "     1) 你确实没改任何会被编译的源文件（正常）" >&2
  echo "     2) 改了但没触发重编（mtime 没刷新）-> 编译源里手动 touch 一下再编" >&2
else
  echo "  OK: vela.bin 已更新"
fi

echo ""
echo "  关键字符串校验（确认新代码真的进了固件）："

# 注意：build.sh 用 unionfs-fuse 做构建视图，刚结束时偶发"stat 已是新文件、
# strings 仍读到旧内容"。所以先取一次，缺就等 2 秒重取，避免误报"没编进去"。

# 标记必须全是 ASCII：strings 默认只识别 7-bit 可打印字符，
# 拿中文当标记会永远报"没编进去"（踩过这个坑）。
MARKERS=(
  "Initializing LCD system"              # lcd.c 开机日志
  "silver_hub"                           # LOG_TAG
  "/dev/uorb/sensor_ambient_temp0"       # sensors.c 温度节点（实测修正后的名字）
  "diag.txt"                             # diag.c 自检报告
)

strings "$VELA" > "${LOG}.strings"
need_retry=0
for marker in "${MARKERS[@]}"; do
  grep -qF "$marker" "${LOG}.strings" || need_retry=1
done

if [ "$need_retry" -eq 1 ]; then
  echo "    (首次未命中，等 2 秒重取一次…)"
  sleep 2
  strings "$VELA" > "${LOG}.strings"
fi

for marker in "${MARKERS[@]}"; do
  if grep -qF "$marker" "${LOG}.strings"; then
    echo "    [有] $marker"
  else
    echo "    [无] $marker   <- 若这是你新加的字符串，说明没编进去"
  fi
done

echo ""
echo "  应用是否注册进 builtin："
FOUND=0
for f in "${WORKSPACE}/apps/builtin/registry/silver_guardian_hub.bdat" \
         "${WORKSPACE}/apps/builtin/registry/contest2026_325_silver_guardian_hub.bdat"; do
  if [ -f "$f" ]; then echo "    [有] $f"; FOUND=1; fi
done
[ "$FOUND" -eq 0 ] && echo "    [无] 没找到 builtin 注册文件 —— 应用可能没进固件"

echo ""
echo "########## 下一步 ##########"
echo "  打包成可烧 .img（别直接跑 pack，那样 fes1/boot0 不是出厂版，烧录会卡 0%）："
echo "    bash tools/pack_hub.sh"
echo "  然后 PhoenixSuit 烧 C:\\Users\\Harryminei\\flash_sg.img（英文路径）"
echo "  详见 docs/开发文档/16_Hub单板功能实现与触摸中文字体修复.md"
