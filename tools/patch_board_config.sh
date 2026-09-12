#!/usr/bin/env bash
#
# 修补板级 defconfig（幂等，可重复执行）
#
# 为什么需要单独一个脚本：板级 defconfig 在 vendor 树里
#   vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/defconfig
# 它由 openvela 的 repo manifest 管理，**不在比赛仓库里**，
# 所以这里的改动没法靠 git 带走 —— 换台机器/重新 repo sync 之后要重跑本脚本。
#
# 改什么、为什么：
#   关掉 CONFIG_LV_FONT_SIMSUN_16_CJK
#     LVGL 自带的示例 CJK 字库只有 1166 个字形，既不够用（"银发守护"缺字），
#     又占约 230KB。应用自带 GB2312 字库之后它就没必要了。
#
# 为什么必须省这点空间：
#   vela.bin 会被打包成 nsh.fex 放进 sys_partition.fex 里的
#   `bootloader` 分区，那个分区只有 16384 扇区 = 8MB。
#   改之前 vela.bin 已经 7.31MB，只剩 715KB 余量，塞不下两个中文字库。
#
# 用法：bash tools/patch_board_config.sh [--check]
#
set -uo pipefail

WORKSPACE="${HOME}/contest2026_325_junweiyanjiuyuan"
DEFCONFIG="${WORKSPACE}/vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/defconfig"

CHECK_ONLY=0
[ "${1:-}" = "--check" ] && CHECK_ONLY=1

if [ ! -f "$DEFCONFIG" ]; then
  echo "找不到板级 defconfig: $DEFCONFIG" >&2
  echo "（需要先 repo sync 出 vendor/allwinnertech）" >&2
  exit 1
fi

changed=0

# ---------------------------------------------------------------- 逐项检查
patch_one() {
  local desc="$1" want="$2" pattern="$3"

  if grep -qE "$pattern" "$DEFCONFIG"; then
    echo "  [已是目标状态] $desc"
  else
    echo "  [需要修改]     $desc"
    changed=1
  fi
}

echo "检查 $DEFCONFIG"
patch_one "关闭 LVGL 自带示例 CJK 字库(省 ~230KB, 应用自带字库)" \
          "" '^# CONFIG_LV_FONT_SIMSUN_16_CJK is not set'

if [ "$CHECK_ONLY" -eq 1 ]; then
  [ "$changed" -eq 0 ] && echo "无需修改" || echo "有 $changed 项待修改（去掉 --check 执行）"
  exit 0
fi

if [ "$changed" -eq 0 ]; then
  echo "无需修改"
  exit 0
fi

# ------------------------------------------------------------------ 应用
cp "$DEFCONFIG" "${DEFCONFIG}.bak_$(date +%Y%m%d_%H%M%S)"

# 1) 如果原来是 =y，先删掉，再补一行 is not set
sed -i '/^CONFIG_LV_FONT_SIMSUN_16_CJK=/d' "$DEFCONFIG"
sed -i '/^# CONFIG_LV_FONT_SIMSUN_16_CJK is not set$/d' "$DEFCONFIG"

# 插到 CONFIG_LV_FONT_MONTSERRAT_* 那一片的后面，保持文件可读
if grep -q '^CONFIG_LV_FONT_MONTSERRAT_8=y' "$DEFCONFIG"; then
  sed -i '/^CONFIG_LV_FONT_MONTSERRAT_8=y/a # CONFIG_LV_FONT_SIMSUN_16_CJK is not set' "$DEFCONFIG"
else
  printf '\n# CONFIG_LV_FONT_SIMSUN_16_CJK is not set\n' >> "$DEFCONFIG"
fi

echo ""
echo "已修改。备份: ${DEFCONFIG}.bak_*"
grep -n 'SIMSUN' "$DEFCONFIG"
echo ""
echo "提示：build.sh 每次会用这个 defconfig 重新生成 .config，直接编译即可生效。"
