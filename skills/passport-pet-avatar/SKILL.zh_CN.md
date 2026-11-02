---
name: passport-pet-avatar
description: 把用户自己的动态角色放到工牌上。用 Codex 的 hatch-pet 技能孵化宠物，把它的图集转换成私有的 76×150 idle/focus/nod 动作包，再编译使用它的固件。不用于静态照片头像、热点头像画廊或发布个人素材。
---

<p align="right"><strong>简体中文</strong> · <a href="SKILL.md">English</a></p>

# 用 Codex 宠物生成工牌动态头像

公开固件自带一个默认动态角色。本流程只在用户本机，把它换成用户在 Codex
里孵化的角色。用 `git rev-parse --show-toplevel` 确定仓库根目录，下文路径都
相对于它。个人素材始终只放在 Git 忽略的位置。

## 1. 在 Codex 里孵化宠物

使用 Codex 内置的 `$hatch-pet` 技能（Pets 功能）。它会在 `~/.codex/pets/<id>/`
生成 v2 宠物：`pet.json` 加一张 8 列、每格 192×208 透明底的图集。请求时让角色
缩小到 76×150 的格子里仍然看得清：

```text
使用 $hatch-pet，根据这张我的照片孵化一个像素风宠物：全身、站立、双脚着地、
轮廓简洁、大致 2:3 的竖向比例，身体旁边不要有大的道具。
```

偏宽或 Q 版的角色也能用，但显示会更小，因为头像格子是竖向的。工牌只用
idle、review 和 waving 三行，审核时重点看这三行。

## 2. 转换并安装

先安装一次宿主依赖（`python -m pip install -r tools/requirements-passport-host.txt`），然后：

```text
python tools/import_codex_pet.py --pet ~/.codex/pets/<id> --install
```

- `idle` → 待机，`review` → 专注，`waving` → 点头回应。不足六帧的行按往返
  循环播放。可用 `--focus-row` / `--nod-row` 改成任意 hatch-pet 行名。
- 18 帧共用同一个裁剪、缩放和脚底基线，切换状态时角色不会忽大忽小。动作包
  写入 Git 忽略的 `assets/private/passport-motion/`。
- `--install` 会把动作包编译进 `build/passport-assets/`（为空时先复制公共素材），
  并在 `private/passport-build.json` 不存在时创建它。已有的个人配置不会被改动；
  这种情况下请按其中的 `asset_dir` 自行运行
  `python tools/build_passport_motion.py --pack assets/private/passport-motion --output <asset_dir>`。

图集尺寸不对或某帧会被裁切时，命令会报错且不写入编译目录；应回到 Codex
修宠物，不要手工改帧。

## 3. 编译与测试

照常编译，不要加公共素材开关：默认固件用 `./tools/validate.sh --firmware`，
Muse 固件用 `pwsh -File tools/build_passport_muse.ps1`（不加 `-PublicAssets`）。
配置指向不完整的素材目录时，CMake 会以 "Required motion assets missing" 停止。
刷机只能通过 `passport-device-test` 并取得用户授权，然后在工牌上确认：身份卡
上循环待机动作，按键时出现专注和点头。

要换回默认角色，删除 `private/passport-build.json`（或用 `-PublicAssets` 编译）。

## 边界

- 绝不提交 `assets/private/`、`private/`、`build/`、原始照片或宠物图集；提交前
  运行 `python tools/check_repo.py`。
- 不发布用个人素材编译的固件；社区发布使用公共素材。
- 编译成功不等于实机验收；大小和动作需要在真实屏幕上确认。
