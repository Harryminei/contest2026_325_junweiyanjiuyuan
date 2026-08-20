# AI Coding 日志系统配置完成

**配置时间**: 2026-08-20
**配置状态**: ✅ 完成
**健康检查**: ✅ 10/10 通过

---

## 🎉 配置成功！

AI Coding 日志采集系统已成功安装并配置完成。

---

## 📊 配置详情

### 安装信息
- **Team ID**: contest2026_325_junweiyanjiuyuan
- **GitHub Login**: Harryminei
- **Demo Repo**: /home/harryminei/contest2026_325_junweiyanjiuyuan
- **Python**: Python 3.12.3

### 已安装组件
- ✅ **全局 Hook 脚本**: ~/.claude/contest-shared/contest-snapshot.sh
- ✅ **快照核心**: ~/.claude/contest-shared/snapshot_core.py
- ✅ **GitHub 登录检测**: ~/.claude/contest-shared/get_github_login.py
- ✅ **Claude Code 配置**: ~/.claude/settings.json
- ✅ **OpenCode 插件**: ~/.config/opencode/plugin/contest-collector.js
- ✅ **MiMo Code 插件**: ~/.config/mimocode/plugins/contest-collector.js
- ✅ **快捷命令**: ~/.local/bin/contest-snapshot
- ✅ **身份配置**: ~/.claude/contest-collector.env
- ✅ **Staging 目录**: ~/.claude/contest-collector-staging

### 健康检查结果
```
Passed: 10
Failed: 0

✅ All checks passed.
```

---

## 🚀 如何使用

### 1. 使用 AI 工具开发

在 **openvela 工作区内**使用以下 AI 工具：

#### Claude Code（推荐）
```bash
# 进入工作区
cd ~/contest2026_325_junweiyanjiuyuan

# 启动 Claude Code
claude

# 开始开发...
# 退出时（/exit 或 Ctrl+D），日志自动保存
```

#### OpenCode
```bash
# 进入工作区
cd ~/contest2026_325_junweiyanjiuyuan

# 启动 OpenCode
opencode

# 开发完成后，日志自动保存
```

#### Codex CLI
```bash
# 进入工作区
cd ~/contest2026_325_junweiyanjiuyuan

# 启动 Codex
codex

# 开发完成后，日志自动保存
```

### 2. 日志自动保存

**重要**: 日志只在 **openvela 工作区内** 自动保存！

识别工作区的方法：
- 存在 `.repo/` 目录
- 在 `~/contest2026_325_junweiyanjiuyuan/` 内

日志保存位置：
```
~/contest2026_325_junweiyanjiuyuan/logs/Harryminei/
├── manifest.json
└── 2026-08-20/
    ├── claude-code__session1.jsonl
    ├── claude-code__session2.jsonl
    └── ...
```

### 3. 提交日志到 GitHub

```bash
# 进入仓库
cd ~/contest2026_325_junweiyanjiuyuan

# 查看日志
ls -la logs/Harryminei/

# 添加日志
git add logs/

# 提交
git commit -s -m "logs: sync AI sessions"

# 推送到 GitHub
git push origin dev-ai-contest-2026
```

---

## 📝 详细操作指南

### 完整开发流程

#### 步骤 1：进入工作区
```bash
# 进入 WSL2
wsl -d Ubuntu-24.04

# 进入工作区
cd ~/contest2026_325_junweiyanjiuyuan

# 加载环境变量
source ~/.bashrc
```

#### 步骤 2：使用 AI 工具开发
```bash
# 启动 AI 工具
claude  # 或 opencode、codex

# 进行开发...
# - 问问题
# - 让 AI 帮你写代码
# - 调试问题
# - 优化代码

# 退出 AI 工具
# Claude Code: /exit 或 Ctrl+D
# OpenCode: exit
# Codex: exit
```

#### 步骤 3：验证日志已保存
```bash
# 查看日志目录
ls -lt logs/Harryminei/ | head -5

# 查看今天的日志
ls -lt logs/Harryminei/2026-08-20/ | head -5
```

#### 步骤 4：提交并推送
```bash
# 添加代码和日志
git add .

# 提交
git commit -m "feat: 实现功能X

- 修改1
- 修改2

AI 日志已自动保存"

# 推送到 GitHub
git push origin dev-ai-contest-2026
```

---

## 🔧 高级功能

### 查看已采集的会话

```bash
# 列出所有会话
contest-snapshot --list

# 预览特定会话
contest-snapshot --session <session-id>

# 预览今天的会话
contest-snapshot --today
```

### 手动导出会话

```bash
# 导出最新会话（预览）
contest-snapshot --latest

# 导出最新会话（确认写入）
contest-snapshot --latest --confirm

# 导出特定会话
contest-snapshot --session <session-id> --confirm

# 导出今天的所有会话
contest-snapshot --today --confirm

# 导出所有会话
contest-snapshot --all --confirm
```

### 查看日志内容

```bash
# 终端预览（彩色）
python3 .claude/skills/contest-log-collector/tools/render-log.py logs/Harryminei/

# 生成 HTML 报告
python3 .claude/skills/contest-log-collector/tools/render-log.py logs/Harryminei/ \
  --format html --out my-report.html
```

### 验证日志合规性

```bash
# 验证日志
python3 .claude/skills/contest-log-collector/tools/validate-log.py logs/
```

---

## ⚠️ 重要提醒

### 1. 工作区限制
- ✅ **在工作区内**：日志自动保存
- ❌ **在工作区外**：不采集日志

**工作区识别**：存在 `.repo/` 目录

### 2. 隐私保护
- 工作区外的对话**完全不采集**
- 个人项目、系统目录、$HOME 等地方的对话不记录
- 只记录 openvela 工作区内的 AI 对话

### 3. 日志内容
记录的内容包括：
- 与 AI 的对话正文 (`text`)
- AI 的思考过程 (`thinking`)
- AI 调用的工具 (`tool_name`, `input`, `output`)
- 使用的模型和 token 统计 (`model`, `tokens_in/out`)
- 会话序号 (`seq`)

### 4. 提交控制
- 工具**只写入本地文件**
- **不会自动执行 git push**
- 你需要手动提交并推送
- 推送前可以删除不想提交的日志文件

### 5. 日志修改限制
- **不要修改日志内容**
- 修改会被检测为作弊
- 如果不想提交某次日志，直接删除文件即可

---

## 🎯 常见问题

### Q1: 日志没有自动保存？
**检查**：
1. 是否在工作区内？
   ```bash
   ls -la ~/contest2026_325_junweiyanjiuyuan/.repo
   ```
2. 是否使用官方支持的 AI 工具？（Claude Code、OpenCode、Codex）
3. 是否在工作区内启动 AI 工具？

### Q2: 如何查看日志？
```bash
# 列出会话
contest-snapshot --list

# 预览内容
contest-snapshot --session <id>

# 查看文件
ls -la logs/Harryminei/
```

### Q3: 如何删除不想提交的日志？
```bash
# 查看日志
ls -la logs/Harryminei/2026-08-20/

# 删除特定日志
rm logs/Harryminei/2026-08-20/<session-file>.jsonl

# 提交时会自动排除已删除的文件
git add logs/
git commit -m "logs: remove unwanted session"
```

### Q4: contest-snapshot 命令找不到？
```bash
# 方法 1：添加 PATH
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc

# 方法 2：使用完整路径
python3 .claude/skills/contest-log-collector/tools/export-session.py --list
```

### Q5: 如何补回历史对话？
```bash
# 补回所有历史对话
contest-snapshot --backfill

# 提交
git add logs/
git commit -s -m "logs: backfill history"
git push
```

---

## 📊 验证清单

配置完成后，确认以下项目：

- [x] ✅ install.sh 运行成功
- [x] ✅ verify-setup.sh 全部通过（10/10）
- [x] ✅ PATH 已配置（~/.local/bin）
- [ ] 测试 AI 工具是否正常采集日志
- [ ] 验证日志文件已生成
- [ ] 测试提交和推送流程

---

## 🎊 配置完成

**AI Coding 日志系统已完全配置完成！**

**现在可以**：
1. 在工作区内使用 AI 工具开发
2. 日志会自动保存到 `logs/` 目录
3. 开发完成后提交并推送到 GitHub

**下一步**：
1. 测试 AI 工具是否正常工作
2. 开始正式开发
3. 定期提交日志到 GitHub

---

## 📚 相关资源

### 官方文档
- **AI Coding 日志指南**: https://github.com/open-vela/docs/blob/dev-ai-contest-2026/zh-cn/contest_2026/ai_coding_log_guide.md
- **参赛代码提交指南**: 参考官方文档

### 本地文档
- **GitHub 推送流程指南.md**: 推送代码和日志的详细流程
- **openvela开发操作手册/**: 完整的开发指南

### 工具命令
```bash
# 查看帮助
contest-snapshot --help

# 查看已采集会话
contest-snapshot --list

# 验证日志
python3 .claude/skills/contest-log-collector/tools/validate-log.py logs/
```

---

## 💡 最佳实践

### 1. 每次开发后提交
```bash
# 开发完成
git add .
git commit -m "feat: 功能X"
git push origin dev-ai-contest-2026
```

### 2. 定期检查日志
```bash
# 查看今天采集的日志
contest-snapshot --today

# 确认日志已保存
ls -lt logs/Harryminei/ | head -5
```

### 3. 保持日志完整
- 不要修改日志内容
- 不要删除已提交的日志
- 如果需要撤回，直接删除文件即可

### 4. 频繁提交
- 小步提交，频繁推送
- 每完成一个功能就提交
- 保持提交历史清晰

---

**配置工程师**: Claude
**配置时间**: 2026-08-20
**版本**: v1.0

**现在可以开始使用 AI 工具开发了！** 🚀

**记得在工作区内使用 AI 工具，日志会自动保存！** ✨
