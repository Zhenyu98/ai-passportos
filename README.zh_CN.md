<p align="right"><strong>简体中文</strong> · <a href="README.md">English</a></p>

<h1 align="center">AI PassportOS</h1>

<p align="center">
  <strong>给 FoloToy AI Passport 用的开源固件，让它变成一张多功能 AI 工牌。</strong>
</p>

<p align="center">
  <strong>动态身份卡</strong> ·
  <strong>Muse AI 助理</strong> ·
  <strong>名片二维码</strong> ·
  <strong>横屏小游戏</strong>
</p>

<p align="center">
  <a href="https://github.com/Zhenyu98/ai-passportos/stargazers"><img alt="GitHub stars" src="https://img.shields.io/github/stars/Zhenyu98/ai-passportos?style=flat-square&logo=github"></a>
  <a href="LICENSE"><img alt="MIT 许可证" src="https://img.shields.io/badge/License-MIT-64748b?style=flat-square"></a>
  <img alt="ESP32-C3" src="https://img.shields.io/badge/ESP32--C3-no_PSRAM-2563eb?style=flat-square">
  <img alt="ESP-IDF" src="https://img.shields.io/badge/ESP--IDF-5.5.3_%7C_6.0.1-14b8a6?style=flat-square">
</p>

<p align="center">
  <img src="assets/images/ai-passportos-hero.png" alt="AI PassportOS 界面：动态身份卡、Muse 助理回复、名片二维码、外滩夜景头像、横屏小游戏" width="94%">
</p>

## 它能做什么

开机是会动的身份卡，按一下就能跟 Muse 说话，闲下来还能把工牌横过来玩两局。全程只用机身上的三个按键。

### 动态身份卡

屏幕上是你的姓名、职位、单位和简介，中英文都行，顶上有时钟和电量。头像是个会呼吸、眨眼、点头的像素小人，也可以换成外滩夜景的动态横幅，排版会跟着头像自己调整。

按 QR 键能全屏亮出名片，比如微信二维码：在手机配置页传一张截图，拖一拖、缩放一下就行。

### Muse AI 助理

在 Muse 手机应用里用蓝牙配对后，按 OK 说话，回复的开头会显示在工牌上，完整内容在手机里看。有用的回复可以存成本地笔记，工牌上能翻最近八条。

<p align="center">
  <img src="assets/images/showcase-muse.png" alt="Muse 助理：录音、回复、任务结果、本地笔记" width="94%">
</p>

Muse 也能反过来操作工牌，但只开放了四件事：查状态、调亮度、存笔记、切页面。

这部分基于 Meta 官方的 [Muse Gadget SDK](https://github.com/facebookincubator/muse-gadget-sdk)，固定在提交 [`e6fe517`](https://github.com/facebookincubator/muse-gadget-sdk/tree/e6fe5174092702ffddae6f90759a38d55a4ebae1)。SDK 在编译时下载，本仓库不附带它的源码。

### 横屏小游戏

菜单里藏了一个横版闯关小游戏。横着拿工牌，上下键左右走，OK 跳，空中还能再跳一次。

<p align="center">
  <img src="assets/images/showcase-game.png" alt="横屏小游戏：游戏中和开始界面" width="66%">
</p>

### 其他

| 功能 | 说明 |
| --- | --- |
| 配置 | 默认固件用手机热点引导页，Muse 固件用带一次性访问码的局域网页面，另有 USB JSON 协议 |
| 网络对时 | Muse 固件连上 Wi-Fi 后自动用 NTP 校准时钟 |
| 自定义角色 | 用 Codex 的 `$hatch-pet` 孵化角色，一条命令转换（[技能说明](skills/passport-pet-avatar/SKILL.zh_CN.md)） |

## 没有 Muse 也能用

发布的固件带 Muse 功能，但不依赖它。没写 token、没配对，或者暂时连不上网时，「打开资料配置页面」会让工牌自己开一个热点，用手机就能改资料、换头像、传名片。

| | 没有 Muse | 配好 Muse 并联网 |
| --- | --- | --- |
| 改资料、头像、名片 | 手机连工牌的热点 `Badge-XXXX` | 同一局域网打开网页，输入屏幕上的临时访问码 |
| Muse AI 助理 | 不可用 | 按 OK 说话，显示回复预览，可存笔记 |
| 时钟 | 在配置页保存资料时用手机时间对准 | 联网后自动对时 |

身份卡、名片二维码和小游戏随时都能用。

## 快速开始

1. 从 [Releases](https://github.com/Zhenyu98/ai-passportos/releases) 下载 `FoloToy-AI-Passport-full.bin`，或自行编译。
2. 从 `0x0` 刷入合并镜像（会重置已保存的数据）：

   ```text
   python -m esptool --chip esp32c3 -p <端口> write-flash 0x0 FoloToy-AI-Passport-full.bin
   ```

3. 在身份卡页长按 OK 两秒，选「Wi-Fi 配置」，再选「打开资料配置页面」：
   - **没联网时**：屏幕显示热点名 `Badge-XXXX`。手机连上这个热点（无密码），设置页会自动弹出；没弹出就打开 `http://192.168.4.1/`。
   - **已经通过 Muse 联网时**：屏幕显示网址、用户名 `passport` 和临时访问码，用同一网络的手机或电脑打开网址登录。

   资料、头像和名片图都在这个页面里改，保存后工牌立即更新。页面开放五分钟后自动关闭，需要时再打开。

想用 Muse AI 助理，按 [Muse 接入指南](docs/assets/passport-muse-integration.zh_CN.md) 写入你自己的 token 并在 Muse 应用里配对。按键操作和 USB 协议见 [使用说明](docs/assets/passport-os-v0.1-user-guide.zh_CN.md)。

## AI 助手一键配置

仓库里有 `AGENTS.md` 和 `skills/`，编程助手读得懂。把下面这段发给 Codex、Claude Code 或别的编程助手：

```text
请阅读 https://github.com/Zhenyu98/ai-passportos 里的 docs/agent-setup.zh_CN.md，
按步骤配置工具链并编译 AI PassportOS。安装任何东西之前先给我看计划，未经我同意
不要刷写设备，最后报告各项编译检查结果。
```

完整清单见 [docs/agent-setup.zh_CN.md](docs/agent-setup.zh_CN.md)。

## Muse 使用须知

- 这是个人开源项目，不是 Meta 做的，也没有得到 Meta 的认可或支持。Muse 是 Meta 的产品和商标。
- 发布的固件里没有 SDK token。请到 [gadgets.muse.ai](https://gadgets.muse.ai/settings/sdk-tokens) 申请你自己的 token，再用 USB 写进工牌，步骤见 [Muse 接入指南](docs/assets/passport-muse-integration.zh_CN.md)。
- token 的使用要遵守 [Gadget SDK 条款](https://gadgets.muse.ai/sdk-terms)：只能自己用，别分享或公开；一个 token 最多绑 50 台设备；装了 token 的设备不能卖，也不能公开上架。

## 已知限制

- Muse 回的是文字。想让工牌念出来，得自己接一个语音合成服务（见[语音说明](docs/assets/passport-agent-voice.zh_CN.md)），仓库里有一个本地测试用的 Windows 桥接。
- ESP32-C3 没有 PSRAM，内存很紧。这一版省出了约 28 KB，Muse 的控制连接已经放得下，但完整的一轮语音对话还没在真机上跑通。
- 长回复、Muse 历史和账号任务都留在手机上，工牌只显示一段预览。

## 致谢

- [FoloToy AI Passport](https://github.com/FoloToy/ai-passport)：硬件、BSP，以及本项目起步用的开源固件模板（MIT）。
- Meta 的 [Muse Gadget SDK](https://github.com/facebookincubator/muse-gadget-sdk)：配对、Wi-Fi 和加密控制连接（Apache-2.0）。编译时按固定提交下载并打补丁，本仓库不附带源码。使用 Muse 须遵守 [Gadget SDK 条款](https://gadgets.muse.ai/sdk-terms)。
- [ESP-IDF](https://github.com/espressif/esp-idf)、[LVGL](https://lvgl.io) 和 [Noto Sans SC](https://fonts.google.com/noto/specimen/Noto+Sans+SC)（OFL）。

## 许可证

MIT，见 [LICENSE](LICENSE)。第三方组件保留各自的许可证。
