[English](passport-agent-voice.md) · **简体中文**

# Agent 动画与回复朗读

Agent 页单独使用 Muse 角色。个人构建为用户 2026-10-24 提供参考图中的 Nora，
身份卡保留原头像。Nora 有八张 128×132 原生帧，包含待机/眨眼、聆听、思考、两种说话嘴型和完成表情。
嘴型跟随实际 PCM 音量，静音时闭嘴；这是音量反馈，不是音素级口型同步。
私有小屏资源生成命令：

```text
python tools/build_passport_agent_avatar.py --sheet assets/private/muse-nora/nora-states-v1.png --output build/passport-assets
```

聆听及朗读动画随实际 PCM 音量联动，音量更新保留回复滚动位置及角色对象。
根据 2026-10-28 要求，Agent 为纯白全屏，只保留 Nora 姓名、角色、字幕和必要
提示，隐藏时钟、电量、卡片、音量柱及长按返回文案。主页按钮压缩至 44 像素，
姓名/单位为 24 像素粗体，单位/标签为 20 像素。菜单及长按 OK 的实际操作保留。

OK 开始录音，再按发送。准备朗读或播放中，OK 停止本地朗读，再按开始新录音。
离开 Agent 页会停止播放，并禁止该待完成回合自动播报；即便在回复前返回，也
不会突然出声。已提交的云端任务继续。朗读失败保留文字；未配置时界面明确提示。

## 语音服务

Muse 提供文字回复。本固件另加 TTS 输出链路，不擅选付费服务，不把 Muse
凭据当作 TTS 凭据使用。

- 配置符合下述协议的 HTTPS 接口后，设备可脱离电脑使用；未预设云端供应商。
- 可选 Windows 桥接使用已安装的中文系统音色，需要同网络的电脑持续运行，
  不属于独立云端服务。

### 可选 Windows 桥接

USB 配置需要 Python 和 `pyserial`。桥接本身仅使用 Python 标准库和 Windows
System.Speech。可以先生成样本，不启动网络服务：

```text
python tools/passport_voice_bridge.py --sample build/chinese-speech.wav
```

需要设备连接时，将示例地址替换为电脑的私有 IPv4 地址，保持进程运行。如
Windows 提示防火墙访问，仅允许预期本地网络，不需要关闭防火墙。

```text
python tools/passport_voice_bridge.py --host 0.0.0.0 --device-host 192.168.1.20
python tools/configure_passport_voice.py --port COM7 --config build/passport-voice.local.json
```

第一条命令在已忽略的本地 JSON 文件中生成随机令牌，不打印令牌。第二条通过
USB 写入已安装新语音固件的设备，不改变 DTR/RTS，也不重启。端口以实际设备
为准。配置保存在 NVS `passport_voice/config`；未启用 Flash/NVS 加密。
本机桥接有认证，私网 HTTP 没有链路加密；远程服务须用 HTTPS。辅助脚本的
PowerShell 执行策略仅作用于子进程，不更改机器或用户策略。

```text
python tools/configure_passport_voice.py --port COM7 --disable
```

此命令只关闭朗读，不改变 Muse 配对、Wi-Fi 或 Passport 资料。

### 接口协议

POST UTF-8 文字，类型为 `text/plain; charset=utf-8`，带
`Authorization: Bearer <私有令牌>`。服务须返回 HTTP 200、
`X-Passport-Audio: pcm_s16le_16000_mono`，以及 16,000 Hz、单声道、16 位小端
PCM 原始音频。接受固定长度及分块响应，拒绝重定向。普通供应商接口如果协议
不同，需要适配器，不能仅填入任意 TTS API 地址。

输入是设备保留的最多 1023 字节回复预览。音频最长 120 秒，设备采用小缓冲，
不将整段音频放入 RAM。连续十秒无数据或单次超过 150 秒会结束朗读。网络调用
使用独立套接字守卫，即使 HTTP 内部持续读取缓慢数据，也会在取消、慢响应头、
数据空闲或绝对期限到达时中止连接。清理客户端前先等待守卫退出；守卫不接触
音频。正常播完后，BSP 会向 DMA 环写入有界静音，确保尾音送出后再休眠；用户
取消时跳过排空。DNS/TLS 时序和 Muse 并发内存峰值仍需设备测试。

## 验证边界

主机测试覆盖 PCM 边界、音频/时间上限、HTTP 格式、认证、提前结束、取消/
离开/返回、文字保留和 UI 对象/定时器稳定。LVGL 截图来自实际应用渲染。
合成 WAV 只验证 Windows 语音生成，不证明 Passport 扬声器或网络正常。
设备声音、延迟、Muse/TTS 并发内存及音质仍需真机验收。构建或配置服务不代表
授权刷机、全局防火墙改动、付费服务或发布凭据。
