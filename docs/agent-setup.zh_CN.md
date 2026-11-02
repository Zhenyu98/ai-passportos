<p align="right"><strong>简体中文</strong> · <a href="agent-setup.md">English</a></p>

# AI 助手配置

## 复制即用的提示词

```text
请阅读 https://github.com/Zhenyu98/ai-passportos 里的 docs/agent-setup.zh_CN.md，
按步骤配置工具链并编译 AI PassportOS。安装任何东西之前先给我看计划，未经我同意
不要刷写设备，最后报告各项编译检查结果。
```

## 目标

在用户电脑上编译出通过校验的 AI PassportOS 固件（默认版或 Muse 版）；未经用户
同意，不碰设备，也不动设备上已保存的数据。

## 前提

- Git、Python 3.11+；Windows 上用 PowerShell 7（Linux 用 `tools/validate.sh`）。
- 默认固件：ESP-IDF 5.5.3（见[环境搭建](development/engineering/environment-setup.zh_CN.md)）。
- Muse 固件：ESP-IDF 6.0.1，以及固定提交的 Muse Gadget SDK（见
  [Muse 指南](assets/passport-muse-integration.zh_CN.md#前提)）。
- 只有用户要配对时才需要 Muse SDK token，由用户通过 USB 自行写入。不要在对话
  里索要，也不要写进仓库里的任何文件。

## 步骤

1. 阅读 `AGENTS.md`，按其要求让 `skills/` 里必需的 `passport-*` 技能可用。先运行
   `git status --short --branch`。
2. 安装宿主依赖：`python -m pip install -r tools/requirements-passport-host.txt`。
3. 运行静态门禁：先 `python tools/prepare_passport_host.py`，再
   `bash tools/validate.sh --static`（Windows 上可用 `tools/validate.ps1 -Mode Static`）。
4. 编译：
   - 默认固件：`bash tools/validate.sh --firmware`；
   - Muse 固件：`pwsh -File tools/build_passport_muse.ps1`（要分享给别人时加
     `-PublicAssets`）。
5. 可选：用 `skills/passport-pet-avatar` 给工牌换上用户自己的动态角色。
6. 只能通过 `skills/passport-device-test` 刷写，且要用户先指定端口并同意。尽量
   用分段刷写保留已存数据。

## 成功标志

- 静态门禁输出 `Repository checks: PASS`，各组宿主测试通过。
- 固件编译输出 `Firmware layout: PASS` 和 `Merged firmware: PASS`。

## 安全规则

- 先检查再动手；安装工具链、刷写、擦除或发布之前都要先问。
- 绝不打印或提交 token、Wi-Fi 密码、个人资料或设备日志。
- 个人素材只放在 `assets/private/`、`private/` 和 `build/`，这些目录都被 Git 忽略。
