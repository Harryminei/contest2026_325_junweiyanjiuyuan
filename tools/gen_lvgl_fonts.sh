#!/usr/bin/env bash
#
# 生成 Silver Guardian Hub 用的 LVGL 中文字库
#
# 为什么要自己生成
# ----------------
# LVGL 内置的 lv_font_simsun_16_cjk 只是示例字库，CJK 部分仅 1166 个字形，
# "银发守护"里的"银/发/护"都不在里面，所以屏幕上只剩"守"字。
# 这里用 GB2312 一级字库（3755 汉字）+ ASCII + 中文标点重新生成。
#
# 字体来源
# --------
# Noto Sans SC（思源黑体的 Google 版本），授权 SIL OFL 1.1 —— 允许嵌入、修改、
# 再分发，包括生成位图字体。**不要用 SimHei/SimSun**：那是中易授权给微软的
# 商业字体，嵌进 Apache-2.0 的开源仓库有授权风险。黑体笔画粗细均匀，
# 在 16px 位图下也比宋体清晰得多。
#
# 前置依赖（一次性）
# ------------------
#   mkdir -p ~/lvfont && cd ~/lvfont && npm install lv_font_conv
#   # 字体会自动下载到 ~/lvfont/src/，也可用 SG_FONT_SRC 指定本地路径
#
# 用法（WSL，仓库根目录）
# ----------------------
#   bash tools/gen_lvgl_fonts.sh            # 生成 16px + 24px 两档
#   bash tools/gen_lvgl_fonts.sh --16       # 只生成 16px
#
# 产物：app/silver_guardian_hub/src/fonts/sg_font_{16,24}.c
# 生成后要跑 tools/sync_app.sh push 同步到编译源再编译。
#
set -uo pipefail

WORKSPACE="${HOME}/contest2026_325_junweiyanjiuyuan"
APP="${WORKSPACE}/app/silver_guardian_hub"
OUTDIR="${APP}/src/fonts"
LVFONT="${HOME}/lvfont/node_modules/.bin/lv_font_conv"

FONT_URL_PRIMARY="https://fastly.jsdelivr.net/gh/notofonts/noto-cjk@main/Sans/SubsetOTF/SC/NotoSansSC-Regular.otf"
FONT_URL_FALLBACK="https://github.com/notofonts/noto-cjk/raw/main/Sans/SubsetOTF/SC/NotoSansSC-Regular.otf"
FONT_DIR="${HOME}/lvfont/src"
FONT_SRC="${SG_FONT_SRC:-${FONT_DIR}/NotoSansSC-Regular.otf}"
FONT_LICENSE="SIL Open Font License 1.1 (Noto Sans SC / Google Noto Project)"

# 字号 -> bpp 映射。取舍依据：
#   16px 是正文主力，用 4bpp 抗锯齿，汉字笔画才分得开；
#   24px 只用于标题，2bpp 省一半体积，大字号下 4 级灰度已够。
SIZES_16="${SG_SIZES_16:-16:4}"
SIZES_24="${SG_SIZES_24:-24:2}"

DO_16=1
DO_24=1
for arg in "$@"; do
  case "$arg" in
    --16) DO_24=0 ;;
    --24) DO_16=0 ;;
    -h|--help) sed -n '2,32p' "$0"; exit 0 ;;
    *) echo "未知参数: $arg" >&2; exit 2 ;;
  esac
done

# ------------------------------------------------------------ 依赖与字体检查
[ -x "$LVFONT" ] || {
  echo "找不到 lv_font_conv: $LVFONT" >&2
  echo "先执行: mkdir -p ~/lvfont && cd ~/lvfont && npm install lv_font_conv" >&2
  exit 1
}

if [ ! -f "$FONT_SRC" ]; then
  mkdir -p "$FONT_DIR"
  FONT_SRC="${FONT_DIR}/NotoSansSC-Regular.otf"
  echo "字体不存在，开始下载 Noto Sans SC (SIL OFL 1.1)..."
  for url in "$FONT_URL_PRIMARY" "$FONT_URL_FALLBACK"; do
    echo "  尝试: $url"
    if timeout 180 curl -sSL --max-time 170 -o "$FONT_SRC.tmp" "$url" \
       && [ "$(stat -c %s "$FONT_SRC.tmp" 2>/dev/null || echo 0)" -gt 1000000 ]; then
      mv "$FONT_SRC.tmp" "$FONT_SRC"
      echo "  OK: $(stat -c %s "$FONT_SRC") bytes"
      break
    fi
    rm -f "$FONT_SRC.tmp"
  done
  [ -f "$FONT_SRC" ] || { echo "字体下载失败。可手动下载后设 SG_FONT_SRC=<路径>" >&2; exit 1; }
fi
echo "源字体: $FONT_SRC  ($(stat -c %s "$FONT_SRC") bytes, $FONT_LICENSE)"
echo ""

# ------------------------------------------------------------------ 字符集
CHARSET="$(python3 "${WORKSPACE}/tools/gen_font_charset.py")" || {
  echo "字符集生成失败" >&2; exit 1; }
echo "字符集: ${#CHARSET} 个字符"
echo ""

mkdir -p "$OUTDIR"
cat > "${OUTDIR}/README.md" <<EOF
# 中文字库（自动生成，请勿手工编辑）

生成命令（在仓库根目录执行）：

    bash tools/gen_lvgl_fonts.sh

- 源字体：Noto Sans SC Regular
- 授权：${FONT_LICENSE}
- 字符集：GB2312 一级汉字(3755) + ASCII + 中文标点，由 \`tools/gen_font_charset.py\` 生成
- 生成工具：lv_font_conv

| 文件 | 变量名 | 字号 | bpp | 用途 |
|---|---|---|---|---|
| sg_font_16.c | \`sg_font_16\` | 16 | 4 | 正文、列表、按钮 |
| sg_font_24.c | \`sg_font_24\` | 24 | 2 | 页面标题 |

改了 \`gen_font_charset.py\` 里的字符集后要重新生成，否则新字会渲染成空白。
EOF

# ------------------------------------------------------------------ 生成
gen_one() {
  local size="$1" bpp="$2" name="sg_font_$1"
  local out="${OUTDIR}/${name}.c"
  echo "--- 生成 ${name}  (${size}px, ${bpp}bpp)"
  "$LVFONT" \
    --font "$FONT_SRC" \
    --size "$size" \
    --bpp "$bpp" \
    --format lvgl \
    --no-compress \
    --symbols "$CHARSET" \
    --lv-font-name "$name" \
    --lv-include lvgl.h \
    -o "$out" || { echo "  生成失败" >&2; return 1; }

  # 统计生成的字形数与位图字节数
  local glyphs bytes
  glyphs="$(grep -c '{.bitmap_index' "$out" 2>/dev/null || echo 0)"
  bytes="$(awk '/glyph_bitmap\[\] = \{/{f=1;next} /^\};/{f=0} f' "$out" | grep -o '0x[0-9a-fA-F]*' | wc -l)"
  printf "  字形 %s 个, 位图 %s 字节 (%.0f KB), 源文件 %.1f KB\n" \
         "$glyphs" "$bytes" "$(echo "$bytes/1024" | bc -l)" \
         "$(echo "$(stat -c %s "$out")/1024" | bc -l)"
}

RC=0
[ "$DO_16" -eq 1 ] && { gen_one ${SIZES_16%%:*} ${SIZES_16##*:} || RC=1; }
[ "$DO_24" -eq 1 ] && { gen_one ${SIZES_24%%:*} ${SIZES_24##*:} || RC=1; }

echo ""
if [ "$RC" -eq 0 ]; then
  echo "完成。产物在 $OUTDIR"
  echo "下一步: bash tools/sync_app.sh push   然后   bash tools/build_hub.sh"
else
  echo "有失败项，见上文。" >&2
fi
exit "$RC"
