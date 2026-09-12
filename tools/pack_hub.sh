#!/usr/bin/env bash
#
# Silver Guardian Hub —— pack 成 PhoenixSuit 可烧的镜像
#
# 为什么不能只跑 `pack` 就完事（这是踩过两次的坑）
# ------------------------------------------------
# 1. **fes1/boot0 必须是出厂版**
#    `pack` 会用编译产物重新生成 fes1.fex / boot0_nand.fex，
#    而 PhoenixSuit 只认出厂固件里那份。用了编译生成的 = **烧录卡 0% 不动**。
#    实测差异：
#      出厂 fes1.fex    21504 B  md5 d63e3a84...
#      pack 生成        20640 B  md5 a9c93f88...
#    所以 pack 之后必须把出厂版覆盖回去，再用 dragon 重新打包。
#    出厂版存在 D:\Desktop\首届openvela比赛\tools\{fes1.fex,boot0_nand.fex}
#    （首次提取方法：用 python 从 gemini_s1_mini.img 取 fes1.fex@0xba400、
#      boot0_nand.fex@0x12c00）
#
# 2. **dragon 要带第二个参数**
#    不带 sys_partition_for_dragon.fex 的话产物只有 2MB。
#
# 3. **镜像要放在纯英文路径**
#    PhoenixSuit 读带中文的路径容易出问题，所以额外拷一份到
#    C:\Users\<你>\flash_sg.img。
#
# 用法（WSL，仓库根目录）：
#   bash tools/pack_hub.sh              # 完整流程
#   bash tools/pack_hub.sh --no-pack    # 跳过 pack，只做 fes1/boot0 修复 + dragon
#
set -uo pipefail

WORKSPACE="${HOME}/contest2026_325_junweiyanjiuyuan"
LICHEE="${WORKSPACE}/vendor/allwinnertech/lichee"
IMGDIR="${LICHEE}/out/r528s3/gemini-s1_nand/image"
IMGNAME="rtos_nuttx_r528s3-gemini-s1_uart0_128Mnand.img"

# 出厂 fes1/boot0 的存放处（Windows 侧）
FACTORY_DIR="/mnt/d/Desktop/首届openvela比赛/tools"
# 纯英文落地路径
WIN_HOME="/mnt/c/Users/Harryminei"
ENGLISH_OUT="${WIN_HOME}/flash_sg.img"

DO_PACK=1
[ "${1:-}" = "--no-pack" ] && DO_PACK=0

# 出厂版的 md5，用来校验替换是否成功（与 09-06 成功烧录那次一致）
FACTORY_FES1_MD5="d63e3a84c7421e13f531e7417cead68d"
FACTORY_BOOT0_MD5="13c5c05a17ceded39c8c39807aca3eb0"

fail() { echo "" >&2; echo "!! $*" >&2; exit 1; }

echo "########## [1/5] 检查出厂 fes1/boot0 ##########"
for f in fes1.fex boot0_nand.fex; do
  [ -f "${FACTORY_DIR}/${f}" ] || fail "找不到出厂 ${FACTORY_DIR}/${f}
   它不在 git 里，需要从出厂固件 gemini_s1_mini.img 提取：
     fes1.fex        @ 0xba400  长度 21504
     boot0_nand.fex  @ 0x12c00  长度 45056"
  echo "  $(md5sum "${FACTORY_DIR}/${f}" | cut -d' ' -f1)  ${f}"
done
echo ""

if [ "$DO_PACK" -eq 1 ]; then
  echo "########## [2/5] pack（会覆盖 fes1/boot0，下一步再修回来）##########"

  # 必须放在子 shell 里跑，两个原因：
  #   1) Allwinner 的 envsetup.sh 会 `set -e`，source 进来之后任何一条命令
  #      返回非 0 都会把整个脚本带崩；
  #   2) `pack` 实际成功（会打印 "pack finish" 并生成镜像）却返回退出码 1，
  #      所以末尾统一 exit 0，由后面的产物校验来把关。
  (
    cd "$LICHEE" || exit 1
    # shellcheck disable=SC1091
    source envsetup.sh >/dev/null 2>&1
    lunch_nuttx r528s3-gemini-s1 >/dev/null 2>&1
    pack 2>&1 | tail -6
    exit 0
  )
  echo ""
else
  echo "########## [2/5] 跳过 pack (--no-pack) ##########"
  echo ""
fi

echo "########## [3/5] 刷新 out/.../image/nsh.fex（关键！）##########"
#
# dragon 打包读的是 out/<board>/image/nsh.fex，而 build.sh 只把新固件写到
#   lichee/board/r528s3/gemini-s1_nand/configs/nsh.fex
# 这两个不是同一个文件，也不会自动同步。
# 踩过的坑：改完代码编译成功、vela.bin 里也确实有新字符串，但 pack 出来的
# 镜像始终是旧的 —— 因为 dragon 一直在读那份没更新的 image/nsh.fex。
# 现象非常迷惑人："编译没问题、strings 也能找到新标记，可板子行为就是没变"。
#
# 所以这里强制覆盖，并且覆盖后立刻校验构建标记。

BOARD_NSH="${LICHEE}/board/r528s3/gemini-s1_nand/configs/nsh.fex"
OUT_NSH="${IMGDIR}/nsh.fex"

[ -d "$IMGDIR" ] || fail "输出目录不存在: $IMGDIR（pack 没跑成功？）"
[ -f "$BOARD_NSH" ] || fail "找不到 $BOARD_NSH（build.sh 没跑到 "Copy nsh.fex" 那步？）"

if [ "$(stat -c %Y "$BOARD_NSH")" -le "$(stat -c %Y "$OUT_NSH" 2>/dev/null || echo 0)" ]; then
  echo "  提示: board/configs/nsh.fex 不比 image/nsh.fex 新，仍强制覆盖以防万一"
fi

cp "$BOARD_NSH" "$OUT_NSH" || fail "复制 nsh.fex 失败"
echo "  $(stat -c %y "$BOARD_NSH" | cut -c1-19)  board/configs/nsh.fex"
echo "  $(stat -c %y "$OUT_NSH"   | cut -c1-19)  image/nsh.fex  (已同步)"

# 校验：新固件里带构建标记，旧的不带。没有标记说明同步没生效。
# 注意 strings 偶尔会读到刚被覆盖前的旧内容（unionfs 视图延迟），
# 所以先取一次，缺就等 2 秒重取，避免误报"同步没生效"吓自己一跳。
strings "$OUT_NSH" > /tmp/nsh_strings.txt
if ! grep -qF "SGHUB-BUILD" /tmp/nsh_strings.txt; then
  echo "  (首次未命中，等 2 秒重取一次…)"
  sleep 2
  strings "$OUT_NSH" > /tmp/nsh_strings.txt
fi

for marker in "SGHUB-BUILD" "/dev/uorb/sensor_ambient_temp0"; do
  if grep -qF "$marker" /tmp/nsh_strings.txt; then
    echo "  [有] $marker"
  else
    echo "  [无] $marker  <- 同步可能没生效" >&2
  fi
done
echo ""

echo "########## [4/5] 把出厂 fes1/boot0 覆盖回去 ##########"
cd "$IMGDIR" || fail "进不去 $IMGDIR"

for f in fes1.fex boot0_nand.fex; do
  cp "${FACTORY_DIR}/${f}" "$f" || fail "复制 $f 失败"
done

M_FES1="$(md5sum fes1.fex | cut -d' ' -f1)"
M_BOOT0="$(md5sum boot0_nand.fex | cut -d' ' -f1)"
echo "  fes1.fex       $M_FES1"
echo "  boot0_nand.fex $M_BOOT0"
[ "$M_FES1" = "$FACTORY_FES1_MD5" ] || fail "fes1.fex 不是出厂版，烧录会卡 0%"
[ "$M_BOOT0" = "$FACTORY_BOOT0_MD5" ] || fail "boot0_nand.fex 不是出厂版，烧录会卡 0%"
echo "  OK: 两个都是出厂版"
echo ""

echo "########## [5/5] dragon 重新打包 ##########"
# 第二个参数不能省，否则产物只有 2MB
"${LICHEE}/tools/tool/dragon" image.cfg sys_partition_for_dragon.fex 2>&1 | tail -6

[ -f "$IMGNAME" ] || fail "没生成 $IMGNAME"
SIZE="$(stat -c %s "$IMGNAME")"
echo ""
echo "  产物: ${IMGDIR}/${IMGNAME}"
echo "  大小: ${SIZE} bytes ($((SIZE / 1024 / 1024)) MB)"
[ "$SIZE" -gt 20000000 ] || fail "镜像只有 $((SIZE / 1024 / 1024))MB，dragon 大概没用对参数"

# 拷贝到所有可能被选中的路径。
#
# 为什么不止拷一份：PhoenixSuit 会记住上次选中的文件，用户习惯性点"刷机"
# 就可能又烧了旧的那个文件 —— 实测因此白排查了一轮（"改了代码但板上行为没变"）。
# 所以把历史用过的文件名也一并覆盖，保证**不管从哪个路径选，都是最新固件**。

cp "$IMGNAME" "$ENGLISH_OUT" && echo "  已拷贝到(英文路径，烧录用这个): ${ENGLISH_OUT}"

ALIASES=(
  "${FACTORY_DIR}/flash_sg.img"
  "${FACTORY_DIR}/flash_20260912_touch_font.img"
  "$(dirname "$FACTORY_DIR")/contest2026_325_junweiyanjiuyuan/firmware/flash_20260912_touch_font.img"
  "${WORKSPACE}/flash_20260912_touch_font.img"
  "${WORKSPACE}/firmware/flash_20260912_touch_font.img"
)

for a in "${ALIASES[@]}"; do
  d="$(dirname "$a")"
  [ -d "$d" ] || continue
  if cp "$IMGNAME" "$a" 2>/dev/null; then
    echo "  已拷贝到: $a"
  fi
done

echo ""
echo "  所有路径的 md5（应完全一致）:"
echo "    $(md5sum "$IMGNAME" | cut -d' ' -f1)  (源)"
for a in "$ENGLISH_OUT" "${ALIASES[@]}"; do
  [ -f "$a" ] && echo "    $(md5sum "$a" | cut -d' ' -f1)  $a"
done

echo ""
echo "########## 下一步：PhoenixSuit 烧录 ##########"
echo "  1. 打开 PhoenixSuit（tools/PhoenixSuit_new/PhoenixSuit.exe）"
echo "  2. 一键刷机 -> 浏览选 C:\\Users\\Harryminei\\flash_sg.img   <- 英文路径"
echo "  3. 模式选「全盘擦除升级」-> 点「刷机」"
echo "  4. 板子断电 3~5 秒再上电（物理触发 FEL，比 adb reboot 可靠）"
echo "  5. 进度到 100% 后板子自动重启"
