#!/usr/bin/env bash
#
# 银发守护 Hub —— 一键编译 + 更新校验
#
# 它解决两个反复踩过的坑：
#   1. 改了代码没同步到编译源 -> 编出来还是旧行为。脚本会先跑 sync_app.sh push。
#   2. 链接失败时 vela.bin 不会更新，但 uname / 编译日志时间戳照旧刷新，
#      看起来"编成功了"，实际烧进去的是旧固件。
#      -> 脚本记录编译前后 vela.bin 的时间戳与大小，并做 strings 校验。
#
# 用法（WSL，仓库根目录）：
#   bash tools/build_hub.sh              # 同步 + 编译 + 校验
#   bash tools/build_hub.sh --no-sync    # 跳过同步，只编译
#   JOBS=16 bash tools/build_hub.sh      # 指定并发（默认 8）
#
# 编完之后还要 pack 成可烧 .img，见 docs/开发文档/15_Hub端完整编译烧录实操手册.md
#
set -uo pipefail

WORKSPACE="${HOME}/contest2026_325_junweiyanjiuyuan"
BOARD_CONFIG="vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/"
JOBS="${JOBS:-8}"
VELA="${WORKSPACE}/nuttx/vela.bin"
LOG="${WORKSPACE}/build_hub_$(date +%Y%m%d_%H%M%S).log"

DO_SYNC=1
for arg in "$@"; do
  case "$arg" in
    --no-sync) DO_SYNC=0 ;;
    -h|--help) sed -n '2,20p' "$0"; exit 0 ;;
    *) echo "未知参数: $arg" >&2; exit 2 ;;
  esac
done

cd "$WORKSPACE" || { echo "工作区不存在: $WORKSPACE" >&2; exit 1; }

# ---------------------------------------------------------------- 1. 同步源码
if [ "$DO_SYNC" -eq 1 ]; then
  echo "########## [1/4] 同步开发源 -> 编译源 ##########"
  bash tools/sync_app.sh push || { echo "同步失败，中止" >&2; exit 1; }
  echo ""
else
  echo "########## [1/4] 跳过同步 (--no-sync) ##########"
  echo "警告：若开发源比编译源新，本次编的是旧代码。"
  echo ""
fi

# ------------------------------------------------------- 2. 记录编译前的基线
BEFORE_MTIME="(不存在)"
BEFORE_SIZE=0
if [ -f "$VELA" ]; then
  BEFORE_MTIME="$(date -r "$VELA" '+%Y-%m-%d %H:%M:%S')"
  BEFORE_SIZE="$(stat -c %s "$VELA")"
fi
echo "########## [2/4] 编译前 vela.bin 基线 ##########"
echo "  时间: $BEFORE_MTIME"
echo "  大小: $BEFORE_SIZE bytes"
echo ""

# ------------------------------------------------------------------ 3. 编译
echo "########## [3/4] 编译 (config=${BOARD_CONFIG}, -j${JOBS}) ##########"
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
echo "########## [4/4] vela.bin 更新校验 ##########"
if [ ! -f "$VELA" ]; then
  echo "  失败：vela.bin 不存在！链接大概率失败了。" >&2
  exit 1
fi

AFTER_MTIME="$(date -r "$VELA" '+%Y-%m-%d %H:%M:%S')"
AFTER_SIZE="$(stat -c %s "$VELA")"
echo "  编译前: $BEFORE_MTIME  (${BEFORE_SIZE} bytes)"
echo "  编译后: $AFTER_MTIME  (${AFTER_SIZE} bytes)"

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
for marker in "Initializing LCD system" "silver_hub" "Silver Guardian Hub"; do
  if strings "$VELA" | grep -qF "$marker"; then
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
echo "  pack 成可烧 .img："
echo "    cd vendor/allwinnertech/lichee"
echo "    source envsetup.sh && lunch_nuttx r528s3-gemini-s1 && pack"
echo "  然后 PhoenixSuit 烧录（详见 docs/开发文档/15_Hub端完整编译烧录实操手册.md）"
