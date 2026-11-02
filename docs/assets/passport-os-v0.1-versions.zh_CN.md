[English](passport-os-v0.1-versions.md) · **简体中文**

# Passport OS 本地版本

当前分支为 `feature/passport-os-v0.1`。源码按完整改动提交；生成固件、私人
资料、照片、个人字库和原始日志保留在忽略的 `build/` 或 `private/`。
当前 `origin` 指向 FoloToy 官方仓库；这些检查点仅保存在本地。

| 本地标签 | 范围 | 验证状态 |
| --- | --- | --- |
| `passport-os-v0.1-baseline` | 源码 `4560a18`：离线徽章、个人资料、主页二维码、USB 配置及复核修正 | 完整检查通过，匹配的合并固件归档已重新核验 |
| `passport-os-v0.1-dev-20261021` | 最新工作快照：Wi-Fi 配置、头像/图片上传、场景画廊、自适应徽章和媒体分区 | 仓库检查与八组 C 测试通过，Python 通过 17/18 项，一项资源测试仍使用旧的缓冲区断言 |

基线合并镜像的准确 SHA-256 为
`ba6b62bd6f12e08e724603e6fa9155d547ddca015fda97fbe4e9dbc6797781e4`，
匹配的 ELF SHA-256 为
`3e4361bf70c18f3a47b93eb4cab236ef07685c38b53d1a75dbd1e1ef31be3067`。
镜像位于忽略的 `build/firmware/` 内容寻址目录中。这是媒体/Wi-Fi 增量之前
的固件，不能当作最新源码已经通过验证的依据。

开发快照用于保存和恢复源码，不是发布版。当时资源生成器报告单帧的
8,192 字节，但实际生成了三份像素缓冲，并增加了 `alpha` 字段，而
`test_generation_is_reproducible_and_frames_are_bounded` 仍断言旧版精确的
24,576 字节字典。升级此快照前，还需核对该测试后续断言、完整静态检查、
当前固件构建及真机验收；旧构建日志不能证明本次新增代码已验证。

可用 `git log --oneline --decorate -10`、`git status --short` 和
`git diff passport-os-v0.1-baseline..HEAD --stat` 查看进度，不丢弃工作文件。
烧录仍需具体构建和数据影响的独立确认。

保存修复对应源码 `2362b95` 和标签 `passport-os-v0.1.1-rc1`，详见[修复验证记录](passport-settings-save-fix.zh_CN.md)。资源总量统计已修正，完整检查通过，真机验收仍待执行。

下一个工作快照为应用版本 `0.1.2`：修复媒体回读（错误的 CRC 初值使所有已
存储的头像场景和图片被静默丢弃），状态标题改为自动消失，缓存像素延迟
释放，互斥量不可用时存储直接拒绝。见[媒体回读修复](passport-settings-save-fix.zh_CN.md#媒体回读修复012)。
