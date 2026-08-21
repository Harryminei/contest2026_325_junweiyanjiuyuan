# Git 同步操作手册

**用途**: WSL 与 GitHub、Windows 本地之间的代码同步
**更新时间**: 2026-08-21

---

## 📋 目录

1. [WSL 推送到 GitHub](#1-wsl-推送到-github)
2. [GitHub 下载到 Windows 本地](#2-github-下载到-windows-本地)
3. [Windows 本地推送到 GitHub](#3-windows-本地推送到-github)
4. [常见问题](#4-常见问题)

---

## 1. WSL 推送到 GitHub

### 步骤 1：进入 WSL

```bash
# 在 Windows CMD 或 PowerShell 中
wsl -d Ubuntu-24.04
```

### 步骤 2：进入项目目录

```bash
cd ~/contest2026_325_junweiyanjiuyuan
```

### 步骤 3：检查状态

```bash
# 查看修改状态
git status

# 查看具体修改
git diff

# 查看提交历史
git log --oneline -5
```

### 步骤 4：添加文件

```bash
# 添加所有修改
git add .

# 或者添加特定文件
git add app/silver_guardian_band/src/main.c
git add docs/
```

### 步骤 5：提交修改

```bash
# 提交
git commit -m "feat: 描述你的修改

- 修改1
- 修改2
- 修改3

Co-Authored-By: Claude Haiku 4.5 <noreply@anthropic.com>"
```

### 步骤 6：推送到 GitHub

```bash
# 推送到远程仓库
git push origin dev-ai-contest-2026
```

### 完整示例

```bash
# 进入 WSL
wsl -d Ubuntu-24.04

# 进入项目
cd ~/contest2026_325_junweiyanjiuyuan

# 查看状态
git status

# 添加修改
git add .

# 提交
git commit -m "feat: 添加新功能"

# 推送
git push origin dev-ai-contest-2026
```

---

## 2. GitHub 下载到 Windows 本地

### 方法 1：克隆仓库（首次）

```bash
# 在 Windows CMD 或 PowerShell 中
cd D:\Desktop\首届openvela比赛

# 克隆仓库
git clone https://github.com/Harryminei/contest2026_325_junweiyanjiuyuan.git

# 进入目录
cd contest2026_325_junweiyanjiuyuan

# 切换到开发分支
git checkout dev-ai-contest-2026
```

### 方法 2：拉取更新（已有仓库）

```bash
# 在 Windows CMD 或 PowerShell 中
cd D:\Desktop\首届openvela比赛\contest2026_325_junweiyanjiuyuan

# 拉取最新代码
git pull origin dev-ai-contest-2026
```

### 方法 3：使用 fetch + merge（推荐）

```bash
# 获取远程更新
git fetch origin

# 查看更新
git log HEAD..origin/dev-ai-contest-2026 --oneline

# 合并更新
git merge origin/dev-ai-contest-2026
```

---

## 3. Windows 本地推送到 GitHub

### 步骤 1：进入项目目录

```bash
# 在 Windows CMD 或 PowerShell 中
cd D:\Desktop\首届openvela比赛\contest2026_325_junweiyanjiuyuan
```

### 步骤 2：检查状态

```bash
git status
git diff
```

### 步骤 3：添加文件

```bash
git add .
```

### 步骤 4：提交修改

```bash
git commit -m "feat: 描述你的修改"
```

### 步骤 5：推送到 GitHub

```bash
git push origin dev-ai-contest-2026
```

---

## 4. 常见问题

### Q1: 推送失败，提示网络错误

**解决方案**：

```bash
# 方法 1：配置代理
git config --global http.proxy http://127.0.0.1:7897
git config --global https.proxy http://127.0.0.1:7897

# 方法 2：使用 SSH（推荐）
# 1. 生成 SSH 密钥
ssh-keygen -t rsa -b 4096 -C "your@email.com"

# 2. 添加到 GitHub
# 复制 ~/.ssh/id_rsa.pub 内容
# 在 GitHub Settings -> SSH Keys 中添加

# 3. 修改远程地址
git remote set-url origin git@github.com:Harryminei/contest2026_325_junweiyanjiuyuan.git

# 4. 推送
git push origin dev-ai-contest-2026
```

### Q2: 拉取时提示冲突

**解决方案**：

```bash
# 方法 1：暂存本地修改
git stash
git pull origin dev-ai-contest-2026
git stash pop

# 方法 2：强制拉取（会丢失本地修改）
git fetch origin
git reset --hard origin/dev-ai-contest-2026
```

### Q3: 如何查看远程仓库地址

```bash
git remote -v
```

### Q4: 如何切换分支

```bash
# 查看所有分支
git branch -a

# 切换分支
git checkout dev-ai-contest-2026

# 创建并切换新分支
git checkout -b feature/new-feature
```

### Q5: 如何撤销修改

```bash
# 撤销工作区修改
git checkout -- <file>

# 撤销暂存
git reset HEAD <file>

# 撤销提交
git reset --soft HEAD~1
```

---

## 📊 工作流程图

```
┌─────────────────────────────────────────────────────────┐
│                    开发流程                              │
└─────────────────────────────────────────────────────────┘

WSL 开发环境                      GitHub 仓库
    │                                 │
    │  git add + commit + push        │
    ├────────────────────────────────→│
    │                                 │
    │                                 │
Windows 本地                        │
    │                                 │
    │  git pull / git fetch + merge   │
    │←────────────────────────────────┤
    │                                 │
    │  git add + commit + push        │
    ├────────────────────────────────→│
    │                                 │
```

---

## 🔧 配置建议

### 1. 配置 Git 用户信息

```bash
git config --global user.name "Harryminei"
git config --global user.email "your@email.com"
```

### 2. 配置代理（如果需要）

```bash
git config --global http.proxy http://127.0.0.1:7897
git config --global https.proxy http://127.0.0.1:7897
```

### 3. 配置 SSH（推荐）

```bash
# 生成密钥
ssh-keygen -t rsa -b 4096 -C "your@email.com"

# 查看公钥
cat ~/.ssh/id_rsa.pub

# 添加到 GitHub
# Settings -> SSH and GPG keys -> New SSH key

# 测试连接
ssh -T git@github.com

# 修改远程地址
git remote set-url origin git@github.com:Harryminei/contest2026_325_junweiyanjiuyuan.git
```

---

## 📝 快速参考

### WSL → GitHub
```bash
cd ~/contest2026_325_junweiyanjiuyuan
git add .
git commit -m "feat: 修改描述"
git push origin dev-ai-contest-2026
```

### GitHub → Windows
```bash
cd D:\Desktop\首届openvela比赛\contest2026_325_junweiyanjiuyuan
git pull origin dev-ai-contest-2026
```

### Windows → GitHub
```bash
cd D:\Desktop\首届openvela比赛\contest2026_325_junweiyanjiuyuan
git add .
git commit -m "feat: 修改描述"
git push origin dev-ai-contest-2026
```

---

**文档维护**: Claude
**最后更新**: 2026-08-21
**版本**: v1.0
