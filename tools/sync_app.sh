#!/usr/bin/env bash
#
# 银发守护 Hub 应用 —— 源码三级同步
#
# 为什么需要这个脚本
# ------------------
# openvela 用 repo manifest 的 <linkfile> 把「比赛仓库里的 app 目录」映射到编译位置：
#     <linkfile src="app/silver_guardian_hub"
#               dest="packages/demos/contest2026_325_silver_guardian_hub"/>
# 也就是说：**实际参与编译的是嵌套目录里的那份拷贝**，而 git 跟踪的是工作区根目录那份。
# 两份不会自动同步。2026-09-06 就是因为这两份脱节，改了 LCD 界面代码却一直编不进固件。
#
# 三级路径
# --------
#   [A] 开发源   ~/contest2026_325_junweiyanjiuyuan/app/silver_guardian_hub
#                （被 git 跟踪，提交/推送到这里）
#   [B] 编译源   ~/contest2026_325_junweiyanjiuyuan/contest2026_325_junweiyanjiuyuan/app/silver_guardian_hub
#                （linkfile 目标，make 真正读的那份）
#   [C] 本地副本 /mnt/d/Desktop/首届openvela比赛/contest2026_325_junweiyanjiuyuan/app/silver_guardian_hub
#                （Windows 侧留存，给你在本地翻阅/编辑用）
#
# 用法（在 WSL 里执行，仓库根目录下）
# ----------------------------------
#   bash tools/sync_app.sh status          # 只看三处差异，不动任何文件
#   bash tools/sync_app.sh push            # A -> B,C  （默认动作；开发完推送）
#   bash tools/sync_app.sh push -n         # 演练，只打印会做什么
#   bash tools/sync_app.sh push --delete   # 顺带删除 B/C 里 A 已不存在的源文件
#   bash tools/sync_app.sh pull            # C -> A   （在 Windows 上改过代码后拉回）
#
# 编译前固定动作：先 push，再 build。忘记 push = 编的是旧代码。
#
set -uo pipefail

WORKSPACE="${HOME}/contest2026_325_junweiyanjiuyuan"
A="${WORKSPACE}/app/silver_guardian_hub"
B="${WORKSPACE}/contest2026_325_junweiyanjiuyuan/app/silver_guardian_hub"
C="/mnt/d/Desktop/首届openvela比赛/contest2026_325_junweiyanjiuyuan/app/silver_guardian_hub"

# 中间产物，三处都不参与同步
EXCLUDES=(
  --exclude='*.o'
  --exclude='*.o.home.*'
  --exclude='Make.dep'
  --exclude='.depend'
  --exclude='.built'
  --exclude='*.bak*'
  --exclude='*.orig'
  --exclude='*.rej'
  --exclude='*.d'
)

ACTION="status"
DRY=""
DELETE=""

for arg in "$@"; do
  case "$arg" in
    status|push|pull) ACTION="$arg" ;;
    -n|--dry-run)     DRY="--dry-run" ;;
    --delete)         DELETE="--delete" ;;
    -h|--help)        sed -n '2,40p' "$0"; exit 0 ;;
    *) echo "未知参数: $arg （-h 看用法）" >&2; exit 2 ;;
  esac
done

# ------------------------------------------------------------------ 前置检查
check_dirs() {
  local missing=0
  for pair in "A:$A" "B:$B" "C:$C"; do
    local label="${pair%%:*}" path="${pair#*:}"
    if [ ! -d "$path" ]; then
      echo "  [缺失] ${label} = ${path}" >&2
      missing=1
    fi
  done
  if [ "$missing" -ne 0 ]; then
    echo "" >&2
    echo "路径不存在。C 需要 Windows 盘挂载（/mnt/d）；B 需要 repo sync 过。" >&2
    exit 1
  fi
}

# diff 没有 --exclude，用它自带的 -x 过滤中间产物（与上面 EXCLUDES 对应）
# --strip-trailing-cr：Windows 侧 core.autocrlf=true 会把文件签出成 CRLF，
# 那是行尾差异不是内容差异，否则 status 会对每个文件都报 differ。
DIFF_X=(-x '*.o' -x '*.o.home.*' -x 'Make.dep' -x '.depend' -x '.built'
        -x '*.bak*' -x '*.orig' -x '*.rej' -x '*.d' --strip-trailing-cr)

show_diff() {
  local from="$1" to="$2" label="$3" out
  out="$(diff -rq "${DIFF_X[@]}" "$from" "$to" 2>/dev/null || true)"
  if [ -z "$out" ]; then
    echo "  ${label}: 一致"
  else
    echo "  ${label}:"
    echo "$out" | sed 's/^/    /'
  fi
}

case "$ACTION" in
  status)
    check_dirs
    echo "三处源码差异（忽略 .o / Make.dep / .depend / .built 等中间产物）："
    show_diff "$A" "$B" "A vs B (开发源 vs 编译源)"
    show_diff "$A" "$C" "A vs C (开发源 vs Windows副本)"
    echo ""
    echo "提示：push 之前若 B 有差异，说明上次改完没同步 —— 编译会用到旧代码。"
    ;;

  push)
    check_dirs
    echo "== A -> B (编译源) =="
    rsync -a --itemize-changes $DRY $DELETE "${EXCLUDES[@]}" "$A/" "$B/"
    echo ""
    echo "== A -> C (Windows副本) =="
    rsync -a --itemize-changes $DRY $DELETE "${EXCLUDES[@]}" "$A/" "$C/"
    echo ""
    if [ -z "$DRY" ]; then
      echo "注意：B 里被更新的源文件 mtime 已刷新，增量编译会重新编译它们。"
    fi
    ;;

  pull)
    check_dirs
    echo "== C -> A (把 Windows 上的改动拉回开发源) =="
    rsync -a --itemize-changes $DRY $DELETE "${EXCLUDES[@]}" "$C/" "$A/"
    echo ""
    if [ -z "$DRY" ]; then
      # Windows 侧 core.autocrlf=true，文件是 CRLF。CRLF 混进 Makefile/Kconfig
      # 会让 make 报 "missing separator" 之类的怪错，所以拉回后统一转回 LF。
      echo "== 去掉 CRLF（只处理文本文件）=="
      find "$A" -type f \
           \( -name '*.c' -o -name '*.h' -o -name '*.txt' -o -name '*.md' \
              -o -name 'Makefile' -o -name 'Make.defs' -o -name 'Kconfig' \
              -o -name 'CMakeLists.txt' -o -name '*.cmake' \) \
           -exec grep -Il $'\r' {} + 2>/dev/null | while read -r f; do
        sed -i 's/\r$//' "$f" && echo "    LF 化: ${f#"$A"/}"
      done
      echo ""
      echo "拉回后记得 git status 看一眼改了什么，再决定提交。"
    fi
    ;;
esac
