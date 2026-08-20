#!/bin/bash
# 09_环境监控脚本.sh
# 用途：自动化检查 openvela 开发环境状态
# 使用方法：chmod +x 09_环境监控脚本.sh && ./09_环境监控脚本.sh

echo "=========================================="
echo "  openvela 开发环境监控脚本"
echo "  版本: 1.0"
echo "  更新: 2026-08-20"
echo "=========================================="
echo ""

# 颜色定义
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 成功计数
pass=0
fail=0
warn=0

check_ok() {
    echo -e "  ${GREEN}✅${NC} $1"
    ((pass++))
}

check_fail() {
    echo -e "  ${RED}❌${NC} $1"
    ((fail++))
}

check_warn() {
    echo -e "  ${YELLOW}⚠️${NC} $1"
    ((warn++))
}

# ==========================================
# 1. 系统环境检查
# ==========================================
echo "1. 系统环境检查"
echo "--------------------------------------------"

# 检查 WSL2
if command -v wsl &> /dev/null; then
    wsl_status=$(wsl -l -v 2>/dev/null | grep -c "Ubuntu-24.04.*Running")
    if [ "$wsl_status" -gt 0 ]; then
        check_ok "WSL2 Ubuntu 24.04 正在运行"
    else
        check_warn "WSL2 Ubuntu 24.04 未运行"
    fi
else
    check_fail "WSL2 未安装"
fi

# 检查用户
user=$(wsl -d Ubuntu-24.04 -- whoami 2>/dev/null)
if [ "$user" = "harryminei" ]; then
    check_ok "用户: harryminei"
else
    check_warn "用户: $user (预期: harryminei)"
fi

# 检查系统信息
kernel=$(wsl -d Ubuntu-24.04 -- uname -r 2>/dev/null)
if echo "$kernel" | grep -q "WSL2"; then
    check_ok "内核: $kernel"
else
    check_warn "内核: $kernel"
fi

echo ""

# ==========================================
# 2. 开发工具检查
# ==========================================
echo "2. 开发工具检查"
echo "--------------------------------------------"

# 检查 GCC
gcc_version=$(wsl -d Ubuntu-24.04 -- gcc --version 2>/dev/null | head -1)
if [ -n "$gcc_version" ]; then
    check_ok "GCC: $gcc_version"
else
    check_fail "GCC: 未安装"
fi

# 检查 ARM GCC
arm_gcc_version=$(wsl -d Ubuntu-24.04 -- arm-none-eabi-gcc --version 2>/dev/null | head -1)
if [ -n "$arm_gcc_version" ]; then
    check_ok "ARM GCC: $arm_gcc_version"
else
    check_fail "ARM GCC: 未安装"
fi

# 检查 Git
git_version=$(wsl -d Ubuntu-24.04 -- git --version 2>/dev/null)
if [ -n "$git_version" ]; then
    check_ok "Git: $git_version"
else
    check_fail "Git: 未安装"
fi

# 检查 Python
python_version=$(wsl -d Ubuntu-24.04 -- python3 --version 2>/dev/null)
if [ -n "$python_version" ]; then
    check_ok "Python: $python_version"
else
    check_fail "Python: 未安装"
fi

# 检查 Make
make_version=$(wsl -d Ubuntu-24.04 -- make --version 2>/dev/null | head -1)
if [ -n "$make_version" ]; then
    check_ok "Make: $make_version"
else
    check_fail "Make: 未安装"
fi

# 检查 CMake
cmake_version=$(wsl -d Ubuntu-24.04 -- cmake --version 2>/dev/null | head -1)
if [ -n "$cmake_version" ]; then
    check_ok "CMake: $cmake_version"
else
    check_warn "CMake: 未安装"
fi

# 检查 Minicom
minicom_version=$(wsl -d Ubuntu-24.04 -- minicom --version 2>/dev/null | head -1)
if [ -n "$minicom_version" ]; then
    check_ok "Minicom: $minicom_version"
else
    check_warn "Minicom: 未安装"
fi

# 检查 Repo
repo_version=$(wsl -d Ubuntu-24.04 -- bash -c "export PATH='\$HOME/.bin:\$PATH' && repo --version 2>&1 | grep 'launcher version'" 2>/dev/null)
if [ -n "$repo_version" ]; then
    check_ok "Repo: $repo_version"
else
    check_fail "Repo: 未安装"
fi

echo ""

# ==========================================
# 3. 环境配置检查
# ==========================================
echo "3. 环境配置检查"
echo "--------------------------------------------"

# 检查 PATH
path_ok=$(wsl -d Ubuntu-24.04 -- bash -c "echo \$PATH | grep -q '.bin' && echo 'yes' || echo 'no'" 2>/dev/null)
if [ "$path_ok" = "yes" ]; then
    check_ok "PATH 包含 ~/.bin"
else
    check_fail "PATH 缺少 ~/.bin"
fi

# 检查 LANG
lang=$(wsl -d Ubuntu-24.04 -- bash -c "echo \$LANG" 2>/dev/null)
if [ "$lang" = "en_US.UTF-8" ]; then
    check_ok "LANG: $lang"
else
    check_warn "LANG: $lang (预期: en_US.UTF-8)"
fi

# 检查 LC_ALL
lc_all=$(wsl -d Ubuntu-24.04 -- bash -c "echo \$LC_ALL" 2>/dev/null)
if [ "$lc_all" = "en_US.UTF-8" ]; then
    check_ok "LC_ALL: $lc_all"
else
    check_warn "LC_ALL: $lc_all (预期: en_US.UTF-8)"
fi

echo ""

# ==========================================
# 4. 工程状态检查
# ==========================================
echo "4. 工程状态检查"
echo "--------------------------------------------"

# 检查工程目录
if wsl -d Ubuntu-24.04 -- test -d ~/contest2026_325_junweiyanjiuyuan; then
    check_ok "工程目录存在"

    # 检查 Repo 初始化
    if wsl -d Ubuntu-24.04 -- test -d ~/contest2026_325_junweiyanjiuyuan/.repo; then
        check_ok "Repo 已初始化"
    else
        check_fail "Repo 未初始化"
    fi

    # 检查目录数量
    dir_count=$(wsl -d Ubuntu-24.04 -- bash -c "ls -d ~/contest2026_325_junweiyanjiuyuan/*/ 2>/dev/null | wc -l" 2>/dev/null)
    if [ "$dir_count" -gt 10 ]; then
        check_ok "工程目录完整 ($dir_count 个子目录)"
    else
        check_warn "工程目录较少 ($dir_count 个子目录)"
    fi

    # 检查 Repo 同步状态
    missing=$(wsl -d Ubuntu-24.04 -- bash -c "export PATH='\$HOME/.bin:\$PATH' && cd ~/contest2026_325_junweiyanjiuyuan && repo status 2>&1 | grep -c 'missing'" 2>/dev/null)
    if [ "$missing" -eq 0 ]; then
        check_ok "代码已完全同步"
    else
        check_warn "有 $missing 个项目待同步"
    fi
else
    check_fail "工程目录不存在"
fi

echo ""

# ==========================================
# 5. 汇总
# ==========================================
echo "=========================================="
echo "  检查结果汇总"
echo "=========================================="
echo ""
echo -e "  ${GREEN}✅ 通过:${NC} $pass"
echo -e "  ${RED}❌ 失败:${NC} $fail"
echo -e "  ${YELLOW}⚠️  警告:${NC} $warn"
echo ""

total=$((pass + fail + warn))
if [ $total -gt 0 ]; then
    percent=$((pass * 100 / total))
    echo "  完成度: $percent%"
else
    echo "  完成度: 0%"
fi

echo ""

if [ $fail -eq 0 ]; then
    if [ $warn -eq 0 ]; then
        echo -e "${GREEN}✅ 环境配置完美！可以开始开发。${NC}"
    else
        echo -e "${YELLOW}⚠️  环境基本就绪，但有一些警告。${NC}"
    fi
else
    echo -e "${RED}❌ 环境配置未完成，请修复失败项。${NC}"
fi

echo ""
echo "=========================================="
echo "  监控完成"
echo "=========================================="

exit 0
