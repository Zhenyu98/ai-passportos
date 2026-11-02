[简体中文](passport-os-v0.1-versions.zh_CN.md) · **English**

# Passport OS local versions

The active branch is `feature/passport-os-v0.1`. Save source changes as focused
commits; generated firmware, private profiles, photos, personal font subsets and
raw logs remain in ignored `build/` or `private/`. The current `origin` points to
the official FoloToy repository; these checkpoints are local.

| Local tag | Scope | Validation |
| --- | --- | --- |
| `passport-os-v0.1-baseline` | Source `4560a18`: offline badge, Profile, homepage QR, USB configuration and review fixes | Full gate passed; matching merged-image archive reverified |
| `passport-os-v0.1-dev-20261021` | New working snapshot: Wi-Fi configuration, uploaded avatars/pictures, gallery, adaptive badge and media partition | Repository checks and eight C groups pass; Python 17/18 pass; one asset metadata assertion remains incompatible with incorrect per-frame metadata |

The baseline's exact merged-image SHA-256 is
`ba6b62bd6f12e08e724603e6fa9155d547ddca015fda97fbe4e9dbc6797781e4`;
matching ELF SHA-256 is
`3e4361bf70c18f3a47b93eb4cab236ef07685c38b53d1a75dbd1e1ef31be3067`.
Its images live under the ignored content-addressed `build/firmware/` directory.
It predates the latest media/Wi-Fi changes and must not be presented as their
validated firmware.

The development snapshot is a recoverable source checkpoint, not a release.
Its asset generator reports 8,192 bytes for one frame although three pixel buffers are emitted and an `alpha`
field, while `test_generation_is_reproducible_and_frames_are_bounded` still
expects the old exact 24,576-byte dictionary. The remaining generator assertions,
full static gate, current firmware build and device acceptance need verification
before promoting this snapshot. Existing older build logs do not validate it.

Use `git log --oneline --decorate -10`, `git status --short`, and
`git diff passport-os-v0.1-baseline..HEAD --stat` to inspect progress without
discarding working files. Device flashing retains its separate exact-build and
data-impact approval requirement.

The save fix is tracked by `passport-os-v0.1.1-rc1` at source `2362b95`; see the [verified candidate](passport-settings-save-fix.md#verified-candidate-2026-10-22). It corrects the metadata total and passes the full gate. Device acceptance remains pending.

The next working snapshot is application version `0.1.2`. It repairs media
readback (a wrong CRC initial value silently discarded every stored avatar scene
and picture), makes status headings expire, releases cached pixels lazily and
fails closed without the storage mutex. See the [media readback repair](passport-settings-save-fix.md#media-readback-repair-012).
