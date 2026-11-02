<p align="right"><strong>简体中文</strong> · <a href="passport-muse-integration.md">English</a></p>

# Passport 接入 Muse

AI PassportOS 也可以作为独立的 ESP-IDF 6.0.1 固件运行在 Muse Gadget SDK 上。本文说明该固件增加了什么、如何构建、如何设置 Muse SDK token、如何在 Muse 手机应用中配对、Muse 可以调用哪些设备工具、数据保存在哪里，以及官方 ESP32-C3 适配没有提供哪些能力。

设备界面与 AI PassportOS 其余部分一样保持简体中文；本文直接使用屏幕上的中文标签。下文的命令是受支持的工作流，本文不是构建或真机测试报告。构建、主机测试、真机测试和未验证项应分别报告。

## Muse 固件新增的能力

- Passport 继续拥有显示、三个按键、本地存储和 ES8311 音频通路。Muse Gadget SDK 负责蓝牙 LE 配对、Wi-Fi、加密控制会话和流式语音笔记。
- 身份卡、AGENT/QR 两个底部按钮、资料、图片页和游戏保持不变。
- AGENT 进入智能体，而不是设置菜单。上下键翻阅智能体文字；OK 根据当前状态确认配对、开始录音、发送录音，或在出错后重试。
- 在身份卡页长按 OK 两秒打开隐藏菜单，包含显示亮度、关于、Wi-Fi 配置、游戏、任务结果、本地笔记和返回。
- 最近一次完成的任务结果和最新八条本地笔记在重启后保留。
- Agent 页增加角色状态动画，并通过已配置的 TTS 接口流式朗读回复；见[Agent 语音配置](passport-agent-voice.zh_CN.md)。不会同步完整的 Muse 账号历史。

默认 ESP-IDF 5.5.3 工程保持不变且仍可构建。它不包含 Muse 传输，因此智能体页会提示需要 Muse 固件。

## 前提

- 一台 FoloToy AI Passport：ESP32-C3、8 MB Flash、无 PSRAM、三个按键，以及能传输数据的 USB 线。
- 运行构建脚本使用 Windows 和 PowerShell。需要 ESP-IDF v6.0.1 及 `esp32c3` 目标；脚本默认到 `..\scratch\esp-idf-v6.0.1` 查找 IDF，到 `..\scratch\idf6-tools` 查找已安装工具。
- 固定版本的 [Muse Gadget SDK](https://github.com/facebookincubator/muse-gadget-sdk) 干净检出，提交为 `e6fe5174092702ffddae6f90759a38d55a4ebae1`，默认放在 `..\scratch\muse-gadget-sdk`。SDK 使用 Apache-2.0，不以源码形式提交进本仓库；构建时复制并打补丁到被忽略的 `build/` 目录。
- Python 3 和 `pyserial`，用于 USB 工具：`python -m pip install pyserial`。
- 在 [gadgets.muse.ai SDK token](https://gadgets.muse.ai/settings/sdk-tokens) 生成 Muse SDK token，并先阅读 [Gadget SDK 条款](https://gadgets.muse.ai/sdk-terms)。每台设备配对都需要 token。
- iOS 或 Android 手机上的 Muse 应用，已登录，并在 **设置 > 设备** 中开启开发者模式（Developer mode）。
- 2.4 GHz Wi-Fi 网络。ESP32-C3 无法连接 5 GHz 网络。

## 构建 Muse 固件

该固件是 `muse/` 下的独立工程。仓库根目录的默认工程仍是 ESP-IDF 5.5.3 构建，不受影响。

### 安装 ESP-IDF v6.0.1

只需安装一次工具链。构建脚本会自行设置 `IDF_TOOLS_PATH`，因此请把工具安装到它默认使用的位置：

```powershell
New-Item -ItemType Directory -Force ..\scratch | Out-Null
git clone -b v6.0.1 --recursive https://github.com/espressif/esp-idf.git ..\scratch\esp-idf-v6.0.1
$env:IDF_TOOLS_PATH = "$((Resolve-Path ..).Path)\scratch\idf6-tools"
..\scratch\esp-idf-v6.0.1\install.ps1 esp32c3
```

### 获取固定版本 SDK

```powershell
New-Item -ItemType Directory -Force ..\scratch | Out-Null
git clone https://github.com/facebookincubator/muse-gadget-sdk.git ..\scratch\muse-gadget-sdk
git -C ..\scratch\muse-gadget-sdk checkout e6fe5174092702ffddae6f90759a38d55a4ebae1
```

### 构建、合并与校验

在仓库根目录用 PowerShell 运行构建脚本：

```powershell
pwsh -File tools\build_passport_muse.ps1
```

脚本会：

1. 导出 ESP-IDF 6.0.1 环境，并拒绝其他 IDF 版本；
2. 把固定版本 SDK 中已跟踪的 `esp32/` 目录复制到 `build/muse-source`，在那里应用 Passport 补丁，不修改 SDK 检出本身；
3. 拒绝不在固定提交、或 `esp32/` 下存在本地改动的 SDK 检出；
4. 把 `muse/` 工程构建到 `build/muse-firmware`；
5. 把 bootloader、分区表和应用程序合并为 `FoloToy-AI-Passport-full.bin`，再运行 `tools/verify_firmware.py` 校验偏移、分区边界、无重叠和 8 MB 上限。

可选参数：

```powershell
pwsh -File tools\build_passport_muse.ps1 -IdfRoot D:\tools\esp-idf-v6.0.1 -SdkRoot D:\src\muse-gadget-sdk
pwsh -File tools\build_passport_muse.ps1 -PublicAssets
```

- `-IdfRoot` 和 `-SdkRoot` 覆盖默认检出位置。
- `-PublicAssets` 强制使用仓库公共素材，而不是通过 `private/passport-build.json` 配置的本机私有素材。
- 脚本会解析 Windows 短路径，因为原生构建工具不接受带空格的路径。
- `build/muse-source` 是生成目录，不要手工修改；应修改固定版本 SDK 检出或补丁脚本后重新构建。
- 脚本只构建和校验，不会刷写设备。

非默认位置仍需满足固定提交和干净检出的检查。如果 `build/muse-source` 不完整，`muse/CMakeLists.txt` 会停止并提示先准备源码。

产物：

| 路径 | 内容 |
| --- | --- |
| `build/muse-firmware/FoloToy-AI-Passport-full.bin` | 合并镜像：bootloader、分区表和应用程序。从 `0x0` 写入。 |
| `build/muse-firmware/FoloToy-AI-Passport.bin` | 仅应用程序镜像。绝不能写到 `0x0`。 |
| `build/muse-firmware/flash_args`、`bootloader/`、`partition_table/` | 供校验脚本使用的 ESP-IDF 构建输出。 |

校验成功会打印 `Firmware layout: PASS (...)` 和 `Merged firmware: PASS (...)`。失败时会打印 `ERROR:` 行并指出失败的镜像或分区，先修复再刷写。

## 刷写固件

刷写前请自行确认具体构建。合并镜像从 `0x0` 写入；由于它同时覆盖 NVS 和 PHY 数据区，可能重置已存数据：设备上保存的 Muse 配对、Wi-Fi 凭据、SDK token、智能体笔记和结果、亮度及资料都可能丢失。若分区布局兼容，用 ESP-IDF 的 `idf.py flash` 分段刷写可以保留 NVS；但构建脚本只为构建解析短路径，因此除非你已有验证过的分段流程，否则建议使用合并镜像。

下面是在已激活的 ESP-IDF 6.0.1 环境中使用其自带 esptool 的示例；请替换串口，并按你安装的 esptool 版本调整参数：

```powershell
python -m esptool --chip esp32c3 --port COM7 --baud 460800 write_flash 0x0 build\muse-firmware\FoloToy-AI-Passport-full.bin
```

刷写具有破坏性，需要明确授权。不要把 `erase-flash` 当作例行步骤，也不要把仅应用程序镜像写到 `0x0`。刷写与已存数据的完整策略见[固件布局](../development/engineering/firmware-layout.zh_CN.md#烧录与已存数据)。

## 设置 Muse SDK token

默认的 Muse 配置不会把 token 编译进镜像，因此刷写后需要通过 USB 控制台设置。token 保存在 NVS 中，从下次启动开始生效。已保存的 token 优先于构建时配置的 token。

```powershell
python tools\configure_passport_muse.py --port COM7
python tools\configure_passport_muse.py --port COM7 --token-file build\muse-token.txt
```

- 不给 `--token-file` 时，脚本会无回显地提示输入 token。请把 token 放在 `build/muse-token.txt` 这类被忽略的文件里，不要提交。
- 脚本通过 USB Serial/JTAG 发送 `>sdk_token=...`，最多等待八秒的 `@sdk_token ok`，设备随后重启。收到 `@sdk_token error` 表示设备拒绝了该 token。
- 脚本不改变 DTR/RTS，不打印 token，且要求已安装 Muse 固件。默认 5.5.3 固件不处理该命令行，脚本会报告没有收到确认。
- 设备只接受完整的 48 字符 SDK token：以 `mgst_` 开头，其余仅含 base64url 字符。脚本自身的前置检查更宽松，实际格式由设备校验，因此被截断或改过的 token 会被拒绝。
- Passport 的 USB 控制台同时承载 JSON 协议，运行命令前请关闭占用该串口的其他监视器。
- 把 token 当作标识符：本项目关闭了 NVS 加密、Flash 加密和 eFuse 写入，因此 token 以明文存于 NVS。token 泄露后请在 gadgets.muse.ai 吊销、生成新 token 并重新设置。
- Muse 在配对时记录 SDK token。若之后更换 token 或配对所用 token 被吊销，需要重新配对；启动日志会在已配对 token 与当前 token 不一致时警告。

## 在 Muse 手机应用中配对

1. 给设备上电。AGENT 页显示「请在手机配对」。
2. 打开 Muse 应用，进入 **设置 > 设备**，开启开发者模式（Developer mode），然后选择 **Add Device**（右上角 `+` 图标）。设备广播名为 `MuseGadget-XXXXXX`。
3. 设备显示配对确认状态「按 OK 确认配对」。按一次 OK，确认是你本人在配对。该按键会被配对流程消费，不会开始录音。
4. 设备加入 Wi-Fi 并连接，状态从「正在连接 Muse」变为「就绪」。若一直停在连接中或「网络已断开」，请在 Muse 应用中为设备配置 Wi-Fi。
5. 配对完成后，Muse 才能调用设备工具。

配对需要这次明确的人工按键，并会建立新的加密会话。社区 SDK 没有厂商验证，无法防止主动的中间人攻击；请在你信任的网络上配网。

网络更换或配对后离线时，在主页长按 OK，进入「Wi-Fi 配置」，选择「重新配对 Muse」。
会出现默认选中取消的二次确认。确认后仅清除 SDK 的 Muse 配对及已保存网络，并重启
进入手机配对；Passport 的身份资料、图片、笔记和任务结果保留。更换 SDK token 后也可
采用这个入口重新配对；该恢复流程不需要擦除 NVS 或整片 Flash。

配对确认绑定于按下 OK 时屏幕显示的 SDK 会话。普通说话点击期间才到达的请求，或替换
之前请求的新会话，不会把旧点击当成配对同意。

## 使用智能体

在身份卡上选中 AGENT 按钮并按 OK 进入智能体。AGENT 页每次显示下列状态之一：

| 状态 | 含义 |
| --- | --- |
| 需要 Muse 固件 | 当前固件没有 Muse 传输。 |
| 请在手机配对 | 设备正在广播，请在 Muse 应用中添加。 |
| 按 OK 确认配对 | 有配对请求在等待，按 OK 确认。 |
| 正在连接 Muse | 配对完成，但 Wi-Fi 或会话尚未就绪。 |
| 就绪 | 可以开始一轮语音。 |
| 网络已断开 | 链路断开或报告错误。 |
| 正在录音 | 正在上传麦克风音频。 |
| Muse 正在处理 | 本轮已提交，Muse 正在处理。 |
| 收到回复 | 页面正在显示回复。 |
| 操作失败 | 上一个动作失败；按 OK 重新开始。 |

智能体页的按键：

- 状态为就绪时按 OK 开始录音；再按 OK 发送。
- 上下键翻阅回复文字。回复超出可视区域时，按键滚动文字而不会切换页面。
- 长按 OK 两秒离开智能体。离开会取消尚未发送的录音；已经提交的回合会继续在 Muse 上处理，之后可在「任务结果」页查看结果。
- 录音中或等待回复时连接中断，本地回合会被取消，页面提示连接中断。
- 录音最长 30 秒。到达上限会自动取消并提示重试；音频以 16 kHz、16 位单声道发送。
- 回复仍在处理时按 OK，页面会提示稍候，而不会开始第二轮。

### 隐藏菜单

在身份卡页长按 OK 两秒（不论当前高亮哪个底部按钮）打开菜单；在其他页面同样长按会返回身份卡。菜单项依次为：

1. **显示亮度** — 在 30%、60%、90% 间循环并保存。
2. **关于** — 产品、版本和板卡信息。
3. **Wi-Fi 配置** — 见下文。
4. **小游戏** — 已有的游戏页。
5. **任务结果** — 最近一次完成的智能体回复。
6. **本地笔记** — 最新八条笔记，每屏一条。
7. **返回** — 回到身份卡。

任务结果页显示最近一次完成的回复。按 OK 可把当前文字保存为本地笔记（以能放进笔记上限为准）；超过 255 字节的结果不能这样保存。笔记页用上下键切换笔记并显示当前位置；保存第九条笔记会丢弃最旧的一条。

### Muse 固件的 Wi-Fi 配置

菜单中的「Wi-Fi 配置」打开「网络与 Muse」，显示设备名、网络、地址和 SDK token 是否
已配置。离线时仍能进入，并通过上面的物理确认流程重新配对。

联网后选择「打开资料配置页面」，可在已加入的网络上开启资料及素材编辑器五分钟。
屏幕持续显示网址、用户名 `passport` 和临时十二位十六进制访问码。在同一局域网打开
网址，把这些凭据填入浏览器登录框。所有注册路由和兜底路由都校验凭据，拒绝其他 Host
或 Origin。访问码每次重新生成、不进入日志，服务关闭后失效。HTTP Basic 凭据通过本地
HTTP 传输，因此应在可信局域网使用。

没有网络时（还没写 token、没有配对或不在信号范围内），同一选项会改为开启工牌自己的
开放热点 `Badge-XXXX`，也就是[使用说明](passport-os-v0.1-user-guide.zh_CN.md)里的引导页。
屏幕显示热点名和 `192.168.4.1`。固件在 SDK 已运行的 Wi-Fi 联网接口旁临时加开热点，
五分钟后恢复为仅联网模式，蓝牙配对和 SDK 的 Wi-Fi 不受影响。若工牌配对过但原网络不在
范围内，联网接口会在后台继续重试，可能让热点信道短暂切换。

工牌第一次拿到地址后启动 SNTP，服务器依次为 `ntp.aliyun.com`、`cn.pool.ntp.org`
和 `pool.ntp.org`。首次同步成功后时钟即视为有效，之后 lwIP 每小时重新对时。在此之前
仍可用 USB `time` 命令或配置页手动校时。

默认 5.5.3 固件仍使用[用户手册](passport-os-v0.1-user-guide.zh_CN.md)中的热点及引导页。

## 设备工具

加密控制会话注册时，固件向 Muse 的注册信息中加入恰好四个 Passport 设备工具。Muse 用 JSON 参数调用它们；固件校验每个参数，并把接受的工作交给应用现有的所有者执行。SDK 自身的标准 Link 命令（例如 `device.health`）仍由 SDK 注册。Passport 的这些工具都不暴露 shell、资料字段、凭据或文件。

会排队执行工作的调用返回下面的确认：

```json
{"ok": true, "payload": {"accepted": true, "status": "queued; not yet applied or persisted"}}
```

`passport.status` 则直接把字段放在 `payload` 中返回。所有失败都使用这种结构：

```json
{"ok": false, "error": {"code": "passport_error", "message": "..."}}
```

| 工具 | 参数 | 行为 |
| --- | --- | --- |
| `passport.status` | 无 | 读取当前设备状态。 |
| `passport.brightness` | `percent`：数字，且必须正好是 30、60 或 90 | 排队修改亮度。 |
| `passport.note` | `text`：非空 UTF-8 字符串，最长 255 字节 | 排队保存一条本地笔记。 |
| `passport.open` | `page`：`badge`、`agent`、`qr`、`menu`、`tasks`、`notes` 之一 | 排队打开某个设备页面。 |

### `passport.status`

只读工具，返回已经生效的状态：

- `battery_percent`：数字；电量读取不可用时为 `null`。
- `brightness_percent`：已生效的亮度，通常为 30、60 或 90。
- `page`：`badge`、`qr`、`menu`、`brightness`、`about`、`game`、`agent`、`tasks`、`notes` 之一。

排队修改亮度或页面后，可轮询该工具确认是否已经生效。`battery_percent` 为 `null` 表示电量计不存在或读数无效，不代表电量为 0。

### `passport.brightness`

- `percent` 必须是 JSON 数字，且精确等于 30、60 或 90。小数值、字符串、`null` 和其他数字都会以 `percent must be exactly 30, 60 or 90` 拒绝。
- 设备输入所有者已停止或队列已满时，调用失败并返回 `Device is busy or stopping; command not accepted`。
- 成功只表示已排队。UI 之后应用该亮度，单写入存储路径再保存；响应不代表屏幕已改变，也不代表已写入 Flash。

### `passport.note`

- `text` 必须是非空 JSON 字符串、合法 UTF-8，且最长 255 字节。空串、缺失或非字符串、非法 UTF-8 以及过长文本会以 `text must be nonempty UTF-8 of at most 255 bytes` 拒绝。
- 设备保留最新八条笔记。每条新笔记成为最新一条；八格已满时丢弃最旧的一条。
- 智能体工作线程停止或任务队列已满时，调用失败并返回 `Note queue is busy; note not accepted`。
- 成功只表示笔记任务已入队。智能体工作线程随后保存笔记并持久化日志；存储失败只在设备屏幕上提示，不会出现在该响应里。

### `passport.open`

- `page` 必填，且必须正好是 `badge`、`agent`、`qr`、`menu`、`tasks`、`notes` 之一。其他值或缺失都会以 `Unknown or missing page` 失败。
- `brightness`、`about`、`game` 可以作为 `passport.status` 的页面值，但 `passport.open` 不接受。
- 输入所有者已停止或队列已满时，调用失败并返回 `Device navigation is busy`。
- 打开页面不会取消已经提交的云端任务。离开智能体页只会取消尚未发送的录音。

### 入队不等于已应用或已持久化

成功响应刻意写明 `queued; not yet applied or persisted`。对亮度、笔记和页面切换来说，工具只是把工作放进了设备内部队列：

- UI 所有者在下个输入周期应用亮度和导航。
- 智能体工作线程应用笔记，并在自己的 NVS 写入中持久化已完成的回复。
- 从接受到持久化之间若发生重启、断电、崩溃或 NVS 写入失败，即使工具返回成功，改动也会丢失。
- 用 `passport.status` 读取已生效的亮度和页面。没有工具能读回已保存的笔记或日志内容。

## 持久化与数据位置

- 智能体数据位于 NVS 命名空间 `passport_agent`，键为 `journal`。该 blob 包含版本、笔记数量、最近一次完成的任务结果（最长 1023 字节）和最新八条笔记（每条最长 255 字节），约 3 KB。
- 在回复完成和保存笔记时写入，启动时读取。恢复时会校验版本和 UTF-8 内容；不兼容或损坏的 blob 会被忽略，日志从空开始。
- 日志在重启以及保留 NVS 的应用刷写后仍在。从 `0x0` 写入合并镜像可能覆盖 NVS 并丢失这些数据。
- 资料字段、图片、头像数据、亮度和 USB 设置仍保存在使用说明描述的既有 Passport 记录和 Flash 分区中。Muse 配对凭据、Wi-Fi 凭据和 SDK token 位于 SDK 的 NVS 记录中。
- 本项目关闭了 NVS 加密、Flash 加密和 eFuse 写入，任何能物理接触设备的人都能读取已存的 Muse token 和 Wi-Fi 凭据。不要在笔记中写入个人秘密。

## 能力边界

本接入提供：

- 通过 Muse 应用完成社区蓝牙 LE 配对，并需要一次明确的人工确认按键。
- 加密控制会话和发往 Muse 的流式语音笔记。
- 设备上的文字回复、滚动翻阅、最近结果页和八条持久本地笔记。
- 四个经过校验的设备工具：状态、亮度、本地笔记和页面导航。
- AI PassportOS 原有的资料、图片、头像和游戏功能，以及复用 Muse 已加入网络的 Wi-Fi 配置页；离线时改用热点。

本接入不提供：

- **没有内置云端 TTS 供应商。** Muse 返回文字；本固件另加有界朗读链路，需要配置语音接口。可选 Windows 桥接需要电脑运行；见[语音服务选项](passport-agent-voice.zh_CN.md)。
- **没有完整长回复或历史同步。** 设备只为显示保留有界回复（约 1 KB），界面始终标明这些内容为预览，并提示到手机查看完整回复。更长的回复、更早的回合、账号任务和 Muse 历史只存在于 Muse 手机应用中；没有任何工具或页面会导入它们。
- **不做云端回滚。** 本地取消、离开智能体或按键都不会撤销 Muse 已经执行的操作。已提交的语音笔记可能已经触发云端动作，排队中的设备命令也不是事务。
- **不会自动重发，也没有离线队列。** 断线重连后失败的回合不会重放，需要用户重试；离线期间语音笔记不会缓存。
- **没有 OTA、视频、Hatch 会话、Flash 加密或 eFuse 功能。** 构建已关闭它们；设备通过 USB 更新。
- **没有任意远程控制。** 上述四个 Passport 工具是唯一新增项；SDK 自身的标准 Link 命令保持不变。Passport 工具不能写资料、凭据或 token，不能执行命令，也不能打开亮度、关于或游戏页。
- **默认固件没有 Muse 传输。** ESP-IDF 5.5.3 工程仍可构建，但其智能体提示需要 Muse 固件，且没有设备工具。

本文未验证：

- Muse 固件的构建、主机测试和真机行为另行报告；本页不记录构建已通过。
- 配对、真实 Muse 回合、回复延迟、内存余量与峰值、麦克风和扬声器质量、Wi-Fi 共存，以及重新配对的恢复路径都需要真机测试。主机测试不等于硬件验收。

## 排障

| 现象 | 检查项 |
| --- | --- |
| 脚本报 `Invalid SDK token format` | token 必须是完整的 48 字符 `mgst_...`，不能含空白；从 gadgets.muse.ai 重新复制。 |
| 脚本报 `Device rejected SDK token` | token 格式错误、已吊销或已过期。生成新 token 后重新设置。 |
| 脚本报 `No token acknowledgement` | 可能未安装 Muse 固件、串口错误，或端口被其他终端占用。关闭 `idf.py monitor` 后重试。 |
| Muse 应用里始终找不到设备 | 先设置 token、开启开发者模式、重启设备，并确认广播名为 `MuseGadget-XXXXXX`。重新添加前先移除应用中的旧条目。 |
| 智能体页提示需要 Muse 固件 | 设备运行的是默认 5.5.3 固件。构建并刷写 Muse 固件。 |
| 页面提示请在手机配对 | 在 Muse 应用中添加设备，出现「按 OK 确认配对」时按 OK。 |
| 一直停在连接中或「网络已断开」 | 在 Muse 应用中为设备配置 Wi-Fi；使用 2.4 GHz 网络并检查信号强度。 |
| 按 OK 只提示稍候 | 回合仍在处理。等待回复或离开页面，不要反复重试。 |
| 回复看起来被截断 | 设备只保留有界回复。完整回答请在 Muse 应用中查看，剩余文字不会同步。 |
| 保存笔记被拒绝 | 文本为空、非法 UTF-8 或超过 255 字节。请保存更短的摘要。 |
| 刷写后笔记和结果消失 | 从 `0x0` 写入的合并镜像可能重置 NVS。改用已验证的分段刷写以保留 NVS，并在完整刷新前导出数据。 |
| 工具返回 `queued...` 但设备无变化 | 请求只是入队，设备可能忙，或之后存储失败。轮询 `passport.status` 并查看设备屏幕提示。 |
| Muse 固件构建失败 | 确认 ESP-IDF 为 6.0.1、SDK 为固定提交的干净检出，并确认默认或正确传入的 `-IdfRoot`/`-SdkRoot`。删除 `build/muse-source` 和 `build/muse-firmware` 后重新构建。 |

## 相关文档

- [Passport Muse 设计](../superpowers/specs/2026-10-23-passport-muse-design.zh_CN.md)与[实施计划](../superpowers/plans/2026-10-23-passport-muse.zh_CN.md)。
- [AI PassportOS v0.1 使用说明](passport-os-v0.1-user-guide.zh_CN.md)：共享应用的按键、资料、图片和 USB 协议。
- [固件布局](../development/engineering/firmware-layout.zh_CN.md)：分区地址、合并镜像与已存数据策略。
- [构建与测试](../development/engineering/build-and-test.zh_CN.md)：默认固件的校验命令。
