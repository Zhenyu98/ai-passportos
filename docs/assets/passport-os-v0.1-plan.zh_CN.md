[English](passport-os-v0.1-plan.md) · **简体中文**

# AI PassportOS v0.1 实施计划

> **供执行代理使用：** 本会话直接执行时，必须使用 superpowers:executing-plans；若用户选择子代理方式，则使用 superpowers:subagent-driven-development。步骤用复选框跟踪。

**目标：** 完成已批准的离线身份卡增量，支持本地配置、资料持久化、真实二维码与小帧头像动画。

**架构：** 纯 C 模型负责校验、导航、记录序列化和二维码布局。ESP-IDF 适配层负责 NVS 与 USB；单一应用所有者更新重新设计的 LVGL 页面。有界工作请求使存储与解析离开按键回调和 LVGL 定时器。

**技术栈：** ESP-IDF 5.5.3、C11、LVGL 9.5.0、现有 BSP、SDK 的 cJSON、Python 3.12 电脑工具，以及 Node.js / lv_font_conv 1.5.3。

**设计：** [已批准设计](passport-os-v0.1-design.zh_CN.md)，用户于 2026-10-20 批准。

## 全局约束

- 基线 `3d685a90bdb96dd8526d84edde1e63fec25b460e`，分支 `feature/passport-os-v0.1`；ESP32-C3、8 MB Flash、无 PSRAM。
- Windows 原生构建，不迁移 WSL，不改全局配置。使用检出目录的无空格短路径别名，使用前验证其指向。
- 保留 BSP 引脚、USB 控制台、分区数据、无关文件与依赖锁；产品 UI 不能沿用基线测试界面。
- 字段字节上限（不含 NUL）：name 64、title 96、research 192、bio 512、homepage 128。主页仅支持有主机名的 HTTP/HTTPS URL。
- NVS 命名空间 `passport`，记录 schema 1；更新被拒或提交失败时保留此前资料/设置。
- 亮度为 30/60/90%，默认 60%；OK 长按 2,000 ms 回身份卡；时间按 UTC+08:00 显示，未校时须明确显示。
- 二维码可用方形 196 像素，四模块静区，整数缩放至少 3；头像为三帧 64×64 RGB565，像素总计不超过 24,576 字节。
- 本轮不启动无线/音频协议栈，不接小智、MCP、Muse、识别或 BLE 改资料。语音笔记等待替换识别服务。
- 私人 JSON、头像、个人生成字体、日志和固件均本地保存并 Git 忽略。不自动刷机、推送或发布。

## 重点复核

1. 两秒长按后的 OK 松手、重复 BSP 事件和队列溢出，不能额外触发短按或永久卡住导航（任务 2、5）。
2. 无效/不完整资料、NVS 损坏和写失败，不能破坏上次记录或其他命名空间（任务 2）。
3. 转义 NUL、JSON 尾随内容、重复键、超大行、旧响应和缺字，必须无副作用地失败，日志不能回显私人值（任务 3）。
4. 短 URL 与最大长度 URL 均须从真实二维码解回原文，并保留四模块静区；更换资料后也要成立（任务 4）。
5. 重复切页与并发配置必须释放页面定时器/缓冲；可选外设或内存分配失败时仍要保持响应（任务 5）。

## 文件划分

| 部分 | 文件与职责 |
| --- | --- |
| 纯应用 | `main/passport_model.[ch]`、`passport_profile.[ch]`、`passport_store.[ch]`、`passport_qr.[ch]`：输入/状态、有界文本、稳定记录、二维码矩阵/布局。 |
| 平台适配 | `main/passport_nvs.[ch]`、`passport_protocol.[ch]`、`passport_usb.[ch]`：NVS 后端、有界 JSON、USB 工作任务。 |
| 显示 | `main/passport_ui.[ch]`、`passport_app.[ch]`、`passport_assets.h`、替换后的 `main/main.c`：页面、生命周期、资产声明、身份卡启动。 |
| 电脑工具 | `tools/test_passport.py`、`configure_passport.py`、`build_passport_assets.py`：测试、USB 配置、资产生成；`tools/preview_passport.c`、`tools/preview/CMakeLists.txt`、`tools/render_passport_preview.py` 用无窗口 LVGL 渲染实际 UI。 |
| 素材/测试 | `assets/passport/ui-text.json`、公共字体/默认帧及许可记录；`tests/test_passport_*` 行为与故障注入测试。 |
| 集成 | `main/CMakeLists.txt`、`sdkconfig.defaults`、`tools/validate.sh`、`.gitignore`、双语素材/使用说明与索引。 |

## 任务 1：准备原生工具并保存基线构建

**文件：** 输入为现有 `tools/validate.sh`、`tools/install_passport_skills.py` 和工程说明。机器专用配置清单与日志保存在被忽略的 `build/setup/`、`build/baseline/`；尚不修改应用源码。

**接口：** 输出已激活且报告 `ESP-IDF v5.5.3` 的 PowerShell 环境、未变化的依赖锁和基线固件/调试包。后续复用此环境，应用不能硬编码机器路径。

- [x] 重新检查现有安装，先运行技能安装器的只读检查。用可读副本或受支持链接安装缺失技能，保留已有目标，再核验五个发现路径。对应阶段阅读 passport-develop / passport-setup。工作区外写入遵循环境审批，不能把读源码说成安装成功。
- [x] 验证短路径确实指向当前仓库。通过官方 Windows 安装流程将 ESP-IDF 5.5.3 并行安装至 无空格的工具目录，使用其 Python 环境；路径不含空格且少于 90 字符。适用时复用内置 Python 3.12 做电脑工具，不替换用户的 Python 3.14。
- [x] 向 Git Bash 提供已校验的原生 C 编译器、Python 与 Windows actionlint 1.7.12，通过仅本次命令的 `CC`、`PATH`、`ACTIONLINT_BIN` 指定。优先复用已有工具；没有编译器时采用官方 LLVM-MinGW 发行包并核对公布的校验值。Windows 不走仅支持 Linux/macOS 的 actionlint 安装回退。记录版本和下载哈希。
- [x] 从短路径、已激活环境执行未修改的 `bash ./tools/validate.sh --static`，再执行 `bash ./tools/validate.sh --firmware`。保存完整退出码、日志和基线归档。预期 Host tests PASS、Firmware build PASS；不通过则先诊断平台兼容问题，不能削弱验证门槛后继续改应用。
- [x] 保存基线提交和依赖锁哈希；堆与真机项目仍标待验。基线构建不刷 COM7，也不证明硬件兼容。

## 任务 2：导航、资料校验与事务记录

**文件：** 新建 `main/passport_model.[ch]`、`passport_profile.[ch]`、`passport_store.[ch]`、`passport_nvs.[ch]`、`tools/test_passport.py`，以及 `tests/test_passport_model.c`、`test_passport_profile.c`、`test_passport_store.c`。

**接口：** `passport_profile_t` 的五字段各为规定上限加 NUL；`passport_record_t` 增加 schema 和亮度。公开 `bool passport_profile_validate(const passport_profile_t *)`、`void passport_model_init(passport_model_t *, uint8_t brightness)`、`passport_action_t passport_model_input(passport_model_t *, passport_key_t, passport_key_event_t, uint64_t now_ms)`、`passport_action_t passport_model_tick(passport_model_t *, uint64_t now_ms)`。动作名为 `PASSPORT_ACTION_NONE`、`PASSPORT_ACTION_RENDER`、`PASSPORT_ACTION_SAVE_BRIGHTNESS`。存储接口为 `passport_status_t passport_store_load(const passport_store_backend_t *, passport_record_t *out)`、`passport_status_t passport_store_save(const passport_store_backend_t *, passport_record_t *current, const passport_record_t *candidate)`。后端含 `ctx`、`read(ctx, uint8_t *buffer, size_t *inout_length)`、`write(ctx, const uint8_t *buffer, size_t length)`，回调均返回 `passport_status_t`。

- [x] 先写失败 C 测试：启动身份卡、循环翻页、Profile 边界滚动、菜单来源恢复、三档亮度；OK 1,999 ms 不返回，2,000 ms 只返回一次，松手不再开菜单。忽略 CLICK/DOUBLE/LONG 通知时，两次 PRESS/RELEASE 仍只产生两次短按。松手丢失后 10 秒安全取消，不能伪造短按。
- [x] 写失败校验/存储测试：每个字段字节边界、多字节边界、坏 UTF-8、空字段、无主机 URL、不支持协议与控制字符；仅 research/bio 允许换行。覆盖 schema/CRC 错、截断记录、提交失败和成功替换；失败保留旧快照，且不调用擦除。
- [x] 运行 `python tools/test_passport.py model profile store` 保存初始失败；实现纯模块、显式序列化的 schema-1 长度/CRC 记录和单键 NVS 适配，不能直接序列化 C 结构填充。`nvs_flash_init()` 失败不触发整片 NVS 擦除。
- [x] 重跑直到所有断言在 `-std=c11 -Wall -Wextra -Werror` 下通过。测试运行器接受组名，从环境选择原生编译器，仅在忽略的 `build/host-tests/` 写产物。
- [x] 审阅差异并保留验证证据。活动流程要求检查点时才做范围明确的本地提交，不自动推送。

## 任务 3：有界 USB 配置

**文件：** 新建 `main/passport_protocol.[ch]`、`passport_usb.[ch]`、`tools/configure_passport.py`、`tests/test_passport_protocol.c`、`tests/test_passport_configure.py`，扩展测试运行器。

**接口：** 使用任务 2 的资料/存储校验。`passport_status_t passport_protocol_parse(const char *line, size_t length, passport_request_t *out)` 只解析、不执行。请求含 uint32 ID、操作与有类型载荷；操作为 `info/get/set/time`，每行上限 8,192 字节。响应前缀 `PASSPORT/1 `，回显 ID 与 `ok`；info 包含 schema、能力和字形清单。字形回调为 `typedef bool (*passport_glyph_fn)(uint32_t codepoint, void *user)`，由任务 4 资产提供。

`passport_reply_t` 表示四种操作的有类型结果；服务钩子为 `typedef passport_status_t (*passport_service_fn)(const passport_request_t *, passport_reply_t *, void *user)`，由 `esp_err_t passport_usb_start(passport_service_fn, void *user)` 绑定，仅在工作任务执行，不能在按键/LVGL 回调运行。`passport_status_t passport_protocol_reply(const passport_reply_t *, char *out, size_t capacity, size_t *written)` 生成有界响应。任务 5 实现服务，并把已提交快照传给 UI 所有者。

- [x] 写失败测试：合法请求、类型/操作错、未知/重复键、解码后 NUL、尾随垃圾、截断/超限行、缺字和时间范围；time 接受 2020-01-01 至 2099-12-31 的 UTC 秒。拒绝请求不能写后端，错误只标字段/错误码，不回显私人值。
- [x] 用假串口写 Python 测试：普通日志、旧 ID、损坏前缀响应、超时和被拒 set 均不能算成功；发送前先做资料及字形校验。必须显式 `--port`，关闭 DTR/RTS，不自动选蓝牙口。
- [x] 运行 `python tools/test_passport.py protocol`、`python -m unittest discover -s tests -p test_passport_configure.py` 建立失败证据；实现 SDK cJSON 解析、原始字符串转义 NUL 检查、有界收行及超限后丢弃到换行恢复。电脑命令为 `info`、`get`、`set --profile`、`time --now`。
- [x] 通过单一有界工作队列与自有不可变载荷传递请求，不保存复用接收缓冲的指针。持久化成功后才向应用发布已提交快照；解析和 NVS 工作不进入 LVGL/按键回调。
- [x] 重跑到 PASS，包含提交故障注入，以及超长行后能处理合法行。新固件获得授权并安装前，串口传输仅用假对象验证。

## 任务 4：真实二维码与可重建资产

**文件：** 新建 `main/passport_qr.[ch]`、`passport_assets.h`、`tools/build_passport_assets.py`、`assets/passport/ui-text.json`、公共默认资产源码、`tests/test_passport_qr.c`、`tests/test_passport_assets.py`。更新双语 `assets/README`；个人生成物放忽略的 `build/passport-assets/`。

**接口：** `bool passport_qr_encode(const char *url, passport_qr_t *out)` 调用已锁定 LVGL qrcodegen，拥有固定 512 字节矩阵缓冲，并输出模块数、整数缩放与居中原点；`bool passport_qr_module(const passport_qr_t *, int x, int y)` 读取模块。`bool passport_assets_has_glyph(uint32_t codepoint, void *user)` 匹配任务 3 回调。生成具有应用生命周期的 `passport_font_16`、`passport_font_20`、字形清单和三帧头像描述符。

- [x] 写失败测试：空/非法 URL 无二维码；公开测试 URL 与合法 128 字节 URL 在 196 像素内以至少 3 倍缩放容纳；四边静区至少四模块；替换地址后矩阵改变。将实际 C 矩阵导出 PGM，用固定版本 zxing-cpp 测试依赖独立解码，结果须等于输入。
- [x] 写资产测试：固定文案、ASCII、个人字符并集；源字体缺字则生成失败；三帧均为 64×64 RGB565，总计不超过 24,576 字节；相同输入再次生成的源码/哈希相同；私人路径被 Git 忽略且不进入可分发源码清单。
- [x] 二维码适配使用 LVGL 9.5.0 qrcodegen、中等纠错、版本 1 至 9；显式计算静区，不直接依赖控件 quiet-zone 选项满足四模块要求。编码器仅链接一次，不修改 managed_components。
- [x] 使用 lv_font_conv 1.5.3 与合法 Noto Sans SC 字体及 OFL 声明，记录来源 URL/版本/哈希和转换器锁。默认只编公共子集；本地 CMake 参数 `PASSPORT_PRIVATE_ASSETS_DIR` 选择个人生成字体/头像，不提交它们。全部文字控件明确绑定生成字体。
- [x] 运行 `python tools/test_passport.py qr`、`python -m unittest discover -s tests -p test_passport_assets.py` 到 PASS。公开二维码预览与资产清单保存在忽略的构建目录。个人头像等待实际文件，默认中性头像明确为临时素材。

## 任务 5：身份卡 UI 与应用生命周期

**文件：** 新建 `main/passport_ui.[ch]`、`passport_app.[ch]`、`tests/test_passport_runtime.c`、`tests/passport_stubs/` 平台桩、`tools/preview_passport.c`、`tools/preview/CMakeLists.txt`、`tools/render_passport_preview.py`；替换 `main/main.c` 启动流程。更新 `main/CMakeLists.txt`、`sdkconfig.defaults`、`.gitignore`、`tools/validate.sh`，保留原有基线测试。

**接口：** `esp_err_t passport_app_start(void)` 初始化应用；`bool passport_ui_create(void)`、`bool passport_ui_render(const passport_model_t *, const passport_record_t *, int battery_soc, bool time_valid, int64_t utc_epoch_s)`、`void passport_ui_destroy(void)` 要求已初始化 LVGL 并持锁。`bool passport_app_post_input(passport_key_t, passport_key_event_t, uint64_t now_ms)` 和已提交快照的所有权有界。渲染后把实测 Profile 滚动上限反馈给模型。预览把实际 UI/资产链接到 LVGL 9.5.0 软件显示，导出 RGB565，仅用 Pillow 编码 PNG。

- [x] 先写失败运行时测试：身份卡启动、缺电量/NVS、开机按住的键须先松开、事件队列溢出恢复、任务/队列/UI 分配失败、反复进入/退出各页。跟踪定时器、页面缓冲和请求，切页后只能保留当前页资源。
- [x] 实现新的身份卡/Profile/主页/菜单/亮度/关于页，使用批准的配色、22 像素安全边距、页码、未设置状态和中文字形。二维码依任务 4 布局绘入小型单色缓冲；删除头像对象前先停定时器；基线 demo 页面不可达。
- [x] 单一启动所有者初始化显示/LVGL、输入、本地存储和可选电量。用 PRESS/RELEASE 时间戳及模型 tick 实现两秒长按，不用 BSP 的 500 ms LONG 事件直接触发。回调非阻塞；队列满时发恢复信号，不静默丢键。不初始化 Wi-Fi、BLE 或音频流。
- [x] 亮度/资料请求由存储工作任务串行处理，提交后才发布快照；保存失败恢复已提交状态并准确提示。校验后设置设备时间，保留本次开机有效标志并按 UTC+08:00 格式化，不改主机全局设置。
- [x] 将专项测试接入现有静态验证，并以故障桩编译真实适配代码。运行 `python tools/test_passport.py all`、Python 工具测试、`bash ./tools/validate.sh --static` 到 PASS。检查启动/导航路径和实际渲染的公开预览，再称 UI 实现完成；真实屏幕验收单独进行。

## 任务 6：构建、审阅与真机准备

**文件：** 输出被忽略的固件/调试包和日志；新建双语 `docs/assets/passport-os-v0.1-user-guide.md` 与索引。更新计划复选框及证据引用，不修改 changelog。

**接口：** 使用前述全部接口和原生环境，交付本次成功的 `build/FoloToy-AI-Passport-full.bin`、符号、分段地址与校验值，失败后不能拿旧镜像当新产物。

- [x] 运行完整 `bash ./tools/validate.sh`、`git diff --check`，审阅源码/依赖锁差异，再用 `python tools/archive_firmware.py verify <exact-successful-bundle>` 验证归档。保存退出码、工具版本、源码提交/dirty 状态、地址和 SHA-256；写设备前按审阅流程复核整个分支。（2026-10-21：静态门经 Git Bash PASS；固件门按 `tools/validate.ps1` 同等步骤在 Windows PowerShell 5.1 下复现通过（绕开 5.1 的 stderr 处理差异）；树干净；归档 `617aae031515fe6c26178a96dab6b4b48995804b4392abb6edd719414a085728` 显式 verify PASS，合并镜像 819,648 字节；证据见 `build/task6-validate-full-20261021.log`；分支 diff 83 个文件已复核。）
- [ ] 检查实际堆/最大块日志与分配边界，分别报告 Build、Host tests、Device tests、Unverified，不能由主机测试或编译成功推断真机通过。
- [ ] 重新枚举目标；COM7 只是此前观察值，不硬编码。写入前识别芯片/Flash、现有分区和数据影响；展示准确已验证产物，再取得此次刷写授权。不强制整片读回、擦除、上传或发布。
- [ ] 获授权刷写后，验证身份卡启动、本地 JSON 导入、重启保留资料、全部长短按、未校时和 USB 校时、中文姓名、手机扫码、亮度保存、动画响应及重复切页。需要个性化验收时再索取资料/素材；无硬件或暂缓刷写时，Device tests 保持 NOT RUN 并列出未验项目。

## 执行交接

用户已于 2026-10-20 批准本计划并选择在当前会话直接执行。实施进行中，计划专用进度记录保存证据与取舍。设计/计划批准已经记录，不再重复确认。

## 执行证据

任务 1–5 已通过 Windows 原生检查：八组应用 C 测试、十八项 Python 工具/资源测试、真实 LVGL 字形检查、600 次页面切换与分配失败注入，以及独立二维码解码。源码检查点为 `faf29bb`。ESP-IDF v5.5.3 安装在 `C:/Espressif`，原 E 盘位置因实测写入延迟已调整。`tools/validate.ps1` 在 Windows 原生环境调用相同检查。2026-10-21 任务 6 第一项完成：完整验证门在源码 `4560a18`（干净树、注入个人资产）上全部 PASS，隔离构建归档 `617aae031515fe6c26178a96dab6b4b48995804b4392abb6edd719414a085728` 显式验证通过，合并镜像 819,648 字节（bootloader@0x0、partition-table@0x8000、app@0x10000）；日志在 `build/task6-validate-full-20261021.log`。仍需：堆/最大块实测复核、刷写授权与真机验收。
