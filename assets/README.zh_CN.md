<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 资源目录（Assets）

本目录集中存放可复用的资源（字库、图片、音乐等），按资源类型分子目录管理。每个资源放在其类型对应的子目录，并记录放置路径、命名方式、集成方式与来源/许可。二进制资源（字体、图片、音频）不属于纯 markdown 文档，请勿与文档混放。涉及版权/授权的资源需注明来源与许可。

## 字库（fonts）

可复用的字库文件与生成的字库源码放在 `fonts/`。

- 命名要能反映字族、字重、字级与格式。
- 记录来源、许可、字符范围、转换命令与目标放置路径。
- 添加字库前评估 Flash 与内部 RAM 影响；ESP32-C3 无 PSRAM。
- 不提交许可不允许分发的字库。

主页姓名与单位使用 `fonts/passport_font_24_bold.c`：同一锁定 Noto Sans SC
字源，24 像素、700 字重、2 bpp，覆盖 6881 个字形，并为 Muse 保留 Flash
空间。`passport/bold-font-manifest.json` 记录源码哈希与参数，使用
`tools/build_passport_bold_font.py` 重建。私有字形清单可通过 `--glyph-manifest`
指定，并输出到忽略目录。

## 图片（images）

AI PassportOS 使用 `fonts/` 中生成的 Noto Sans SC 16/20 像素、4 bpp 子集，
同目录保留 OFL 许可。源文件固定为 Google Fonts 提交
`2894aab31764f10f29c421bdfd2340d3b382d384`，SHA-256 为
`a3041811a78c361b1de50f953c805e0244951c21c5bd412f7232ef0d899af0da`。
`fonts/passport-font-converter.lock.yaml` 固定 lv_font_conv 1.5.3。
运行 `python tools/build_passport_assets.py --font FONT --font-conv CONVERTER_JS
--node NODE --output assets` 可复现公共资源。固定文案位于
`passport/ui-text.json`，`passport/manifest.json` 记录字形和哈希。
生成个人字形或头像时，增加 `--profile build/profile.local.json` 和可选的
`--avatar IMAGE`，并使用 `--output build/passport-assets`；个人输出必须保持
Git 忽略。默认中性头像为原创程序生成的临时素材，采用仓库许可，
包含三个 64 × 64 RGB565 帧。二维码使用锁定的 LVGL 编码器和四模块留白。

可复用的源图与生成的显示资产放在 `images/`。

角色动画的公共空集源码为 `images/passport_motion.c`；用户确认的个人 18 帧 RGBA
源图位于忽略的 `private/passport-motion/`。个人图来自用户角色参考与 2026-10-22
内置 image_gen 生成，不假定拥有再分发授权。使用 `tools/build_passport_motion.py`
打包到忽略的 `build/passport-assets`；格式与选择方法见[动画接入说明](../docs/assets/passport-motion.zh_CN.md)。
确认的夜景/撸猫源帧保存在忽略的 `private/passport-scenes/`，个人资源重新生成时会一起带入。
普通个人构建通过仓库根目录下忽略的 `private/passport-build.json` 要求三组动态入口齐全，个人形象素材不再分发。

Muse Agent 单独使用 Nora，身份卡头像不变。2026-10-24 用户提供的截图保存在
忽略的 `private/muse-nora/reference.jpg`；内置 `imagegen` 生成透明八帧图
`private/muse-nora/nora-states-v1.png`，保留眼镜、双马尾、米色发结与浅蓝衣服，
起初不假定再分发授权。`tools/build_passport_agent_avatar.py` 将素材打包为八张
128×132 RGB565A8 帧，放入忽略的 `build/passport-assets/images/passport_agent_avatar.c`。
像素共 405,504 字节，驻留 Flash，不使用整帧解码缓冲。2026-11-02 作者同意公开这个形象：公共构建的
`images/passport_agent_avatar.c` 使用同一组八帧，页面显示名为 Muse。素材表在
`showcase/agent/muse-states.png`，可用 `python tools/build_passport_agent_avatar.py
--sheet assets/showcase/agent/muse-states.png --output assets --name Muse --public`
重新生成；原始截图 `reference.jpg` 仍不公开。本地 `required_agent_avatar: true`
与源码哈希检查防止个人构建静默丢失 Nora。

| 文件 | 尺寸与格式 | 用途与来源 |
| --- | --- | --- |
| [`images/home.jpg`](images/home.jpg) | 3840 × 2160，JPEG | 嵌入中英文项目 README 的产品主图，突出 AI Passport 产品形象与开放、人人可创作的理念。 |
| [`images/readme-hardware-specs.png`](images/readme-hardware-specs.png) | 2172 × 724，PNG RGBA | 保留为可选技术参考图，不再用于首页主视觉。于 2026-09-17 使用内置图像生成工具为本仓库生成；已根据文档中的硬件能力契约核对图中的六项标签与参数。 |
| [`images/logo-wordmark.png`](images/logo-wordmark.png) | 1648 × 336，PNG RGBA | 从仓库原始 `images/logo.png` 中精确裁切并去除背景的黑色字标；用于中英文项目 README 的浅色主题。 |
| [`images/logo-wordmark-dark.png`](images/logo-wordmark-dark.png) | 1648 × 336，PNG RGBA | 提取字标的白色版本；README 使用 `<picture>` 在 GitHub 深色主题下显示。 |

- 使用描述性命名，并记录尺寸、像素格式、转换步骤与目标路径。
- 优先采用适合 240 × 320 RGB565 显示的格式，并纳入 Flash 与内部 RAM 考量。
- 许可允许时保留可编辑源文件，并记录来源与许可。
- 图片中不得包含设备二维码秘密、凭证或个人数据。

## 音乐与音效（music）

可复用的音乐与音效源码放在 `music/`。

- 记录来源、许可、采样率、位深、声道、转换命令与目标路径。
- 与当前 BSP 音频路径匹配时优先采用 16 kHz、16 位单声道 PCM。
- 嵌入音频前评估 Flash 与内部 RAM 成本；长录音应流式或分块。
- 无再分发许可不提交媒体文件。
