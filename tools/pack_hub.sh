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

echo "########## [1/4] 检查出厂 fes1/boot0 ##########"
for f in fes1.fex boot0_nand.fex; do
  [ -f "${FACTORY_DIR}/${f}" ] || fail "找不到出厂 ${FACTORY_DIR}/${f}
   它不在 git 里，需要从出厂固件 gemini_s1_mini.img 提取：
     fes1.fex        @ 0xba400  长度 21504
     boot0_nand.fex  @ 0x12c00  长度 45056"
  echo "  $(md5sum "${FACTORY_DIR}/${f}" | cut -d' ' -f1)  ${f}"
done
echo ""

if [ "$DO_PACK" -eq 1 ]; then
  echo "########## [2/4] pack（会覆盖 fes1/boot0，下一步再修回来）##########"
  cd "$LICHEE" || fail "lichee 目录不存在"
  # shellcheck disable=SC1091
  source envsetup.sh >/dev/null 2>&1
  lunch_nuttx r528s3-gemini-s1 >/dev/null 2>&1
  pack 2>&1 | tail -6
  echo ""
else
  echo "########## [2/4] 跳过 pack (--no-pack) ##########"
  echo ""
fi

echo "########## [3/4] 把出厂 fes1/boot0 覆盖回去 ##########"
[ -d "$IMGDIR" ] || fail "输出目录不存在: $IMGDIR（pack 没跑成功？）"
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

echo "########## [4/4] dragon 重新打包 ##########"
# 第二个参数不能省，否则产物只有 2MB
"${LICHEE}/tools/tool/dragon" image.cfg sys_partition_for_dragon.fex 2>&1 | tail -6

[ -f "$IMGNAME" ] || fail "没生成 $IMGNAME"
SIZE="$(stat -c %s "$IMGNAME")"
echo ""
echo "  产物: ${IMGDIR}/${IMGNAME}"
echo "  大小: ${SIZE} bytes ($((SIZE / 1024 / 1024)) MB)"
[ "$SIZE" -gt 20000000 ] || fail "镜像只有 $((SIZE / 1024 / 1024))MB，dragon 大概没用对参数"

# 拷到纯英文路径 + D:/tools
cp "$IMGNAME" "$ENGLISH_OUT" && echo "  已拷贝到(英文路径，烧录用这个): ${ENGLISH_OUT}"
cp "$IMGNAME" "${FACTORY_DIR}/flash_sg.img" && echo "  已拷贝到: ${FACTORY_DIR}/flash_sg.img"

echo ""
echo "########## 下一步：PhoenixSuit 烧录 ##########"
echo "  1. 打开 PhoenixSuit（tools/PhoenixSuit_new/PhoenixSuit.exe）"
echo "  2. 一键刷机 -> 浏览选 C:\\Users\\Harryminei\\flash_sg.img   <- 英文路径"
echo "  3. 模式选「全盘擦除升级」-> 点「刷机」"
echo "  4. 板子断电 3~5 秒再上电（物理触发 FEL，比 adb reboot 可靠）"
echo "  5. 进度到 100% 后板子自动重启"
