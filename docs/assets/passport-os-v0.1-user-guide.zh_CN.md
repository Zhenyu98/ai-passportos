<p align="right"><strong>简体中文</strong> · <a href="passport-os-v0.1-user-guide.md">English</a></p>

# AI PassportOS v0.1

设备开机即进入科研徽章页。上/下键在两个底部按钮间移动高亮，确定键进入高亮的按钮：AGENT 进入智能体页，QR 进入全屏图片页（显示上传的名片/二维码图，无顶部状态栏）。图片页按确定键回到徽章页。徽章布局随头像自动适配：竖版站立场景为左侧头像栏 + 右侧信息列；宽幅或近方形场景为上层大头像 + 下方紧凑信息。真实头像带柔和的呼吸式透明度脉动；未设置时使用会眨眼的内置临时头像。亮度页用上/下键在 30%、60%、90% 间循环并持久保存。在身份卡页长按确定两秒打开隐藏菜单；其他页面长按确定两秒返回身份卡页。未设置的字段显示占位提示。时钟在每次开机校时前显示 `--:--`，校时后显示 UTC+08:00；电量读取失败显示 `--%`。工牌没有独立供电的时钟芯片，用电源键关机后时间会丢失。Muse 固件连上 Wi-Fi 后会自动向网络时间服务器对时；默认固件需要用 USB `time` 命令或配置页的手机校时按钮。

## 智能体与隐藏菜单

智能体页取决于所刷固件。默认 ESP-IDF 5.5.3 构建没有 Muse 传输，会提示需要 Muse 固件；Muse 固件增加蓝牙 LE 配对、流式语音笔记和文字回复，构建、token 与配对步骤见 [Muse 接入指南](passport-muse-integration.zh_CN.md)。

- 确定键执行屏幕提示的动作：确认待确认的物理配对、开始录音、发送录音，或在出错后重新尝试。
- 上/下键翻阅回复或转写文字。任务结果页显示最近一次完成的回复，按确定键可把当前文字保存为本地笔记（以能放进笔记上限为准）。笔记页用上/下键切换最新八条笔记。
- 离开智能体会取消尚未发送的录音；已经提交的回合会继续处理，结果仍可在任务结果页查看。
- 单次录音最长 30 秒。
- 在身份卡页长按确定两秒打开隐藏菜单，不受当前高亮按钮影响；其他页面同样长按返回身份卡页。菜单共七项：显示亮度、关于、Wi-Fi 配置、小游戏、任务结果、本地笔记、返回。上/下键移动行高亮，确定键进入高亮项；视口每次显示四行并随高亮滚动。

## Wi-Fi 手机配置

Muse 固件会先打开「网络与 Muse」，离线仍可通过二次确认重新配对。资料编辑器复用
当前局域网，并使用屏幕上显示的临时访问码，详见 [Muse 指南](passport-muse-integration.zh_CN.md)。
下面的热点步骤适用于默认 5.5.3 固件。

在隐藏菜单（身份卡页长按确定两秒）中选择「Wi-Fi 配置」。徽章开启热点 `Badge-XXXX`（开放网络，启动五分钟后关闭），内置 DNS 把所有域名解析到自身并重定向系统联网探测，因此手机连上热点会自动弹出设置页（Android / iOS / Windows；未弹出时手动访问 `http://192.168.4.1/`）。

页面可编辑五个资料字段、用手机时钟校时、切换头像。内置像素场景画廊（私有构建为你完整的 32 场景头像包，公共构建为「动态角色」和「外滩夜景」两个动态展示场景）以按钮列表呈现，点选即把该场景写入头像槽位、徽章立即显示。图片页的名片图（例如微信名片/二维码）同样通过手机上传：页面给出 240×320 的整页裁剪框，可拖动、缩放选择要保留的区域，浏览器把裁剪结果转成 RGB565，设备直接从 Flash 内存映射渲染、整屏显示且不占 RAM。「清除主页图片」可回到空状态，选择「动态角色」可恢复默认头像。保存走与 USB 相同的单写者存储路径并做字形校验，包含未编译字符的文本会被拒绝。热点在启动五分钟后关闭。

## USB 配置

用 pip 安装 `tools/requirements-passport-host.txt`。必须指定核实后的串口，下列 `COM7` 仅为示例。工具不自动探测串口，打开前将 DTR/RTS 设为关闭。UTF-8 JSON 文件必须包含五个字符串字段：`name`、`title`、`research`、`bio`、`homepage`，均可为空。字节上限依次为 64/96/192/512/128，不含末尾零字符。只有研究方向和介绍允许换行，主页为空或有效的 HTTP(S) 地址。

```text
python tools/configure_passport.py --port COM7 info
python tools/configure_passport.py --port COM7 set --profile build/profile.local.json
python tools/configure_passport.py --port COM7 get
python tools/configure_passport.py --port COM7 time --now
```

PC 端还有一个 `tools/passport_web.py` 本地网页工具（资料表单、头像包场景选择、自定义上传并渲染预览），仅作开发辅助，不在设备上运行。

导入会检查编译的字形清单，新增汉字需重新生成字库并构建；不支持的文字会被拒绝。个人输入、生成资源和预览放在忽略的 `build/`。资料通过 USB 单独写入本应用的 NVS 记录，保存失败保留已提交数据；不会用擦除 NVS 来恢复。

## 复现与验收

按[资产说明](../../assets/README.md)固定字体/许可与本地资产。CMake 默认使用公共资产；`PASSPORT_PRIVATE_ASSETS_DIR` 选择忽略的本地输出。先用 `python tools/prepare_passport_host.py` 准备哈希锁定的宿主源码，再运行 `bash tools/validate.sh --static`。门禁包含真实 LVGL 渲染、字形编译校验、600 次翻页、内存池稳定性和独立解码的二维码像素。

本用户固件在 factory 应用之后追加了 512 KB 的 `avatar` 数据分区；NVS 偏移不变，因此分段烧录可保留已存数据。

已激活的原生 Windows ESP-IDF 5.5.3 环境可用 `tools/validate.ps1 -Mode All -HostPython HOST_PYTHON -BuildDirectory NEW_DIRECTORY`，可选 `-PrivateAssetsDirectory LOCAL_ASSETS` 与 `-BashExecutable GIT_BASH`。它运行同样的静态门禁、构建、合并、镜像校验和归档，避开 SDK 不支持的 MSYS 路径。Linux 使用 `./tools/validate.sh`。

归档保留配套的 ELF/MAP、组件镜像、烧录参数和哈希。启动日志报告剩余堆与最大可用块。真实板载数值、栈余量、中文显示、按键、持久化与手机扫码需要真机验收；宿主通过不等于硬件验收。

烧录前需确认具体的已验证构建，并先检查设备现有分区表。兼容的分段烧录可保留 NVS；从 0 偏移烧录的合并镜像会覆盖 NVS 并可能重置数据。不做全片擦除、原厂固件备份、上传或发布。设备背面的无源 NTAG213 标签可用手机一次性写入主页链接实现碰一碰分享，与固件无关。


## iPhone 的 NFC 碰一碰设置

保存个人主页字段不会写入背面的无源 NTAG213，MCU 没有写此标签的接口。
因此标签里没有可用网址记录时，iPhone 直接靠近仍可能无法使用，即使固件和主页链接均正常。
标签为空或格式不兼容目前只是推测，需要手机读写 App 实读确认。

先用手机 NFC 读写 App 读取标签。初始化分享可用 NFC Tools：**写入 → 添加记录 → URL/URI**，
填入同一完整 `https://` 主页网址，手机 NFC 天线贴近背面 NFC 标记完成写入。
随后读回核对网址，退出读写 App，在亮屏状态下再次靠近。支持后台读取的 iPhone（XS 及更新机型）
要求可识别的 NDEF URI 记录；把网址写成纯文本不等同于网址记录。
参见 [Apple 后台读取要求](https://developer.apple.com/documentation/corenfc/adding-support-for-background-tag-reading)
和 [NFC Tools 官方写入步骤](https://www.wakdev.com/en/knowledge-base/how-to-guides/how-to-write-a-link-url-on-an-nfc-chip.html)。

如果读写 App 也完全检测不到标签，先调整天线位置，移开手机壳或夹在中间的卡片重试，
并记录 iPhone 型号及 App 结果。此时需检查射频/标签，修改工牌固件不能写入或修复这枚独立无源标签。
以后修改主页链接，若要 NFC 保持一致，也需重新写入标签网址。
