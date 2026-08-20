# GitHub 推送流程指南

**项目**: 2026首届openvela AI硬件开发者大赛
**队伍**: 325 junweiyanjiuyuan
**GitHub**: https://github.com/Harryminei/contest2026_325_junweiyanjiuyuan

---

## 🎯 概述

本文档说明如何在开发完成后将代码推送到 GitHub，包括：
- Windows 本地与 Ubuntu WSL2 的同步
- AI Coding 日志的提交
- 代码的推送流程

---

## 📋 前置条件

### 已完成配置
- ✅ GitHub 仓库已克隆到 Windows 本地
- ✅ Ubuntu WSL2 中的 openvela 工程已同步
- ✅ Git 用户信息已配置
- ✅ 远程仓库已连接（origin → Harryminei）

### 目录结构
```
D:\Desktop\首届openvela比赛\
├── contest2026_325_junweiyanjiuyuan/  ← Windows 本地仓库
│   ├── app/
│   ├── board/
│   ├── quickapp/
│   ├── logs/
│   └── ...
│
└── openvela开发操作手册/              ← 操作指南

WSL2 Ubuntu:
~/contest2026_325_junweiyanjiuyuan/    ← WSL2 中的工程
├── .repo/                            ← Repo 工程标识
├── nuttx/
├── apps/
└── contest2026_325_junweiyanjiuyuan/  ← 符号链接到实际代码
```

---

## 🔄 开发工作流

### 流程图

```
1. 在 Ubuntu WSL2 中开发
        ↓
2. 使用 AI 工具（Claude Code 等）
        ↓
3. AI 日志自动保存到 logs/
        ↓
4. 代码修改完成
        ↓
5. 同步到 Windows 本地
        ↓
6. 在 Windows 中提交并推送到 GitHub
        ↓
✅ 完成
```

---

## 📝 详细步骤

### 步骤 1：在 Ubuntu WSL2 中开发

```bash
# 进入 WSL2
wsl -d Ubuntu-24.04

# 进入工程目录
cd ~/contest2026_325_junweiyanjiuyuan

# 开始开发
# 使用 AI 工具（Claude Code、OpenCode 等）
claude  # 或其他 AI 工具
```

**重要提醒**：
- 在 openvela 工作区内使用 AI 工具，日志会自动保存
- 代码修改完成后，记录你做了哪些改动

---

### 步骤 2：同步代码到 Windows 本地

#### 方法 1：使用 Git 同步（推荐）

在 Ubuntu WSL2 中：
```bash
# 查看修改
git status

# 添加修改的文件
git add .

# 提交（本地提交，还未推送）
git commit -m "描述你的修改"
```

在 Windows 本地（Git Bash 或 PowerShell）：
```bash
# 进入本地仓库
cd "D:\Desktop\首届openvela比赛\contest2026_325_junweiyanjiuyuan"

# 拉取 WSL2 中的修改
# 注意：需要先配置 WSL2 为远程仓库，或使用共享目录
```

#### 方法 2：使用共享目录（简单）

WSL2 可以直接访问 Windows 文件系统：
```bash
# 在 WSL2 中，修改直接保存到 Windows 目录
cd /mnt/d/Desktop/首届openvela比赛/contest2026_325_junweiyanjiuyuan

# 或者使用符号链接
ln -s /mnt/d/Desktop/首届openvela比赛/contest2026_325_junweiyanjiuyuan ~/win_contest
```

#### 方法 3：使用 rsync 或 cp

```bash
# 从 WSL2 复制到 Windows
cp -r ~/contest2026_325_junweiyanjiuyuan/app /mnt/d/Desktop/首届openvela比赛/contest2026_325_junweiyanjiuyuan/

# 或使用 rsync
rsync -avz ~/contest2026_325_junweiyanjiuyuan/ /mnt/d/Desktop/首届openvela比赛/contest2026_325_junweiyanjiuyuan/
```

---

### 步骤 3：在 Windows 中提交并推送

```bash
# 进入本地仓库
cd "D:\Desktop\首届openvela比赛\contest2026_325_junweiyanjiuyuan"

# 查看状态
git status

# 添加所有修改
git add .

# 查看将要提交的内容
git diff --cached --stat

# 提交
git commit -m "feat: 描述你的修改

- 修改1
- 修改2
- 修改3"

# 推送到 GitHub（推送到你自己的仓库，不合并）
git push origin dev-ai-contest-2026
```

---

## 🤖 AI Coding 日志提交

### 日志位置

```
contest2026_325_junweiyanjiuyuan/
└── logs/
    └── Harryminei/                   ← 你的 GitHub 用户名
        ├── manifest.json
        └── 2026-08-20/
            ├── claude-code__session1.jsonl
            ├── claude-code__session2.jsonl
            └── ...
```

### 自动日志收集

在 Ubuntu WSL2 中使用 AI 工具时：
- 日志会**自动保存**到 `logs/` 目录
- 无需手动导出
- 会话结束时自动写入

### 提交日志

```bash
# 在 Windows 本地
cd "D:\Desktop\首届openvela比赛\contest2026_325_junweiyanjiuyuan"

# 添加日志
git add logs/

# 提交日志
git commit -m "logs: sync AI sessions"

# 推送
git push origin dev-ai-contest-2026
```

### 验证日志

```bash
# 查看日志文件
ls -la logs/Harryminei/

# 查看最新日志
ls -lt logs/Harryminei/2026-08-20/ | head -5
```

---

## 🔧 配置 WSL2 与 Windows 的同步

### 方案 1：使用共享目录（推荐）

在 Ubuntu WSL2 中直接操作 Windows 目录：
```bash
# 创建符号链接
ln -s /mnt/d/Desktop/首届openvela比赛/contest2026_325_junweiyanjiuyuan ~/contest

# 开发时使用符号链接
cd ~/contest
# 修改会直接保存到 Windows 目录
```

### 方案 2：配置 Git远程仓库

在 Ubuntu WSL2 中添加 Windows 仓库为远程：
```bash
# 在 WSL2 中
cd ~/contest2026_325_junweiyanjiuyuan

# 添加 Windows 仓库为远程
git remote add win /mnt/d/Desktop/首届openvela比赛/contest2026_325_junweiyanjiuyuan

# 推送到 Windows
git push win dev-ai-contest-2026

# 在 Windows 中拉取
cd "D:\Desktop\首届openvela比赛\contest2026_325_junweiyanjiuyuan"
git pull origin dev-ai-contest-2026
```

### 方案 3：使用 Git远程（高级）

直接从 WSL2 推送到 GitHub：
```bash
# 在 WSL2 中配置 GitHub 认证
gh auth login

# 直接推送
git push origin dev-ai-contest-2026
```

---

## 📊 推送检查清单

在推送前，确认以下事项：

### 代码检查
- [ ] 代码可以正常编译
- [ ] 功能测试通过
- [ ] 没有语法错误
- [ ] 代码格式规范

### 日志检查
- [ ] AI Coding 日志已保存到 logs/
- [ ] 日志文件完整（.jsonl 格式）
- [ ] 日志目录结构正确

### Git 检查
- [ ] `git status` 显示工作区干净
- [ ] 提交信息清晰明了
- [ ] 没有敏感信息泄露

### 推送检查
- [ ] 远程仓库配置正确
- [ ] 分支正确（dev-ai-contest-2026）
- [ ] 有推送权限

---

## 🎯 常见推送场景

### 场景 1：日常开发推送

```bash
# 1. 在 WSL2 中开发完成
# 2. 同步到 Windows
# 3. 在 Windows 中提交
git add .
git commit -m "feat: 实现功能X"
git push origin dev-ai-contest-2026
```

### 场景 2：提交 AI 日志

```bash
# 1. 使用 AI 工具开发
# 2. 日志自动保存到 logs/
# 3. 在 Windows 中提交日志
git add logs/
git commit -m "logs: sync AI sessions"
git push origin dev-ai-contest-2026
```

### 场景 3：修复 Bug

```bash
# 1. 在 WSL2 中修复 Bug
# 2. 测试通过
# 3. 同步到 Windows
# 4. 提交修复
git add .
git commit -m "fix: 修复XXX问题"
git push origin dev-ai-contest-2026
```

### 场景 4：更新文档

```bash
# 1. 更新 README 或文档
# 2. 同步到 Windows
# 3. 提交文档
git add README.md
git commit -m "docs: 更新项目文档"
git push origin dev-ai-contest-2026
```

---

## ⚠️ 注意事项

### 1. 先提交到自己的仓库
- 推送到 `origin/dev-ai-contest-2026`
- **不要**直接合并到主分支
- 等待评审后再合并

### 2. 提交信息规范
使用清晰的提交信息：
```
feat: 新功能
fix: 修复 Bug
docs: 文档更新
style: 代码格式
refactor: 重构
test: 测试相关
chore: 构建/工具链
logs: AI 日志
```

### 3. 频繁提交
- 小步提交，频繁推送
- 每完成一个功能就提交
- 保持提交历史清晰

### 4. 备份重要代码
- 推送到 GitHub 就是备份
- 重要修改先提交再继续
- 避免丢失工作成果

---

## 🔍 验证推送成功

### 检查 GitHub

```bash
# 在浏览器中打开
# https://github.com/Harryminei/contest2026_325_junweiyanjiuyuan

# 查看最新提交
# 查看 logs/ 目录
# 确认文件已上传
```

### 命令行验证

```bash
# 查看远程状态
git remote -v

# 查看分支
git branch -a

# 查看最新提交
git log --oneline -5

# 查看与远程的差异
git status
```

---

## 💡 最佳实践

### 1. 开发前
```bash
# 拉取最新代码
git pull origin dev-ai-contest-2026

# 创建功能分支（可选）
git checkout -b feature/my-feature
```

### 2. 开发中
```bash
# 频繁提交
git add .
git commit -m "feat: 部分功能实现"

# 推送到远程
git push origin dev-ai-contest-2026
```

### 3. 开发后
```bash
# 确保所有修改已提交
git status

# 推送最终版本
git push origin dev-ai-contest-2026

# 验证推送成功
git log --oneline -3
```

---

## 📞 遇到问题？

### 推送失败
```bash
# 检查网络
ping github.com

# 检查认证
gh auth status

# 重新认证
gh auth login
```

### 冲突解决
```bash
# 拉取最新
git pull origin dev-ai-contest-2026

# 解决冲突
# 编辑冲突文件

# 提交解决
git add .
git commit -m "merge: 解决冲突"
git push origin dev-ai-contest-2026
```

### 日志未保存
```bash
# 检查是否在工作区内
pwd

# 检查 .repo 目录是否存在
ls -la ~/contest2026_325_junweiyanjiuyuan/.repo

# 查看日志目录
ls -la logs/
```

---

## 🎊 完成推送

推送成功后：
1. ✅ 代码已保存到 GitHub
2. ✅ AI 日志已提交
3. ✅ 工作已备份
4. ✅ 可以继续开发

**下一步**：
- 继续开发新功能
- 或等待评审反馈

---

**文档版本**: v1.0
**更新时间**: 2026-08-20
**维护者**: Claude

**祝开发顺利！** 🚀
