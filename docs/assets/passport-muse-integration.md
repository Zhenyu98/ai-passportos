<p align="right"><a href="passport-muse-integration.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Passport Muse integration

AI PassportOS can also run on the Muse Gadget SDK as a separate ESP-IDF 6.0.1
firmware. This guide covers what that firmware adds, how to build it, how to
set the Muse SDK token, how to pair the badge in the Muse app, which device
tools Muse may call, where data is stored, and which capabilities the official
ESP32-C3 port does not provide.

The device UI stays Simplified Chinese, as in the rest of AI PassportOS; this page
translates the labels it refers to. The commands below describe the supported
workflow; this page is not a build or device-test report. Report Build, Host
tests, Device tests and Unverified results separately.

## What the Muse firmware adds

- Passport keeps ownership of the display, the three buttons, local storage and
  the ES8311 audio path. The Muse Gadget SDK supplies Bluetooth LE pairing,
  Wi-Fi, the encrypted control session and streamed voice notes.
- The badge, its AGENT and QR buttons, the profile, the picture page and the
  game are unchanged.
- AGENT opens the assistant instead of the settings menu. UP/DOWN scroll the
  assistant text; OK confirms pairing, starts a recording, sends it, or retries
  after an error.
- The hidden menu opens with a two-second OK hold on the badge. It contains
  display brightness, about, Wi-Fi configuration, the game, task results, local
  notes and return.
- The last completed task result and the newest eight local notes survive a
  reboot.
- The Agent page adds character state animation and optional streamed reply
  speech through a configured TTS endpoint. See [Agent voice setup](passport-agent-voice.md).
  Complete Muse account history is not synchronized.
- Voice notes go to the badge's own Muse side chat rather than the main chat,
  and the badge subscribes only to that chat. The conversation appears as a side
  chat in the Muse app and keeps its own history.
- Chat subscriptions can deliver frames of about 16 KB whatever the reply's
  length: on device, an `approvals.snapshot` event listing recent approvals
  arrives right after subscribing, even in a new side chat. The SDK's 12 KB session
  buffers for this board cannot hold such a frame, and a larger frame ends the
  session and loses the reply, so this build raises them to
  16.6 KB and frees the difference from Passport's own RAM: the USB line buffers
  and the game state are allocated only while used. Chat rows too long to show are
  skipped and logged.
- The screen turns off after the SDK's stored idle setting (`sleep_s`, 120 seconds
  by default; 0 means never). Passport replaces the SDK's own UI, so it applies
  that setting itself. While the screen is off, Wi-Fi drops to the SDK's
  `MUSE_WIFI_REST` level, which keeps the association, network time and control
  session with up to about a second of extra latency. See the
  [user guide](passport-os-v0.1-user-guide.md) for what wakes the screen.

The default ESP-IDF 5.5.3 project remains unchanged and buildable. It contains
no Muse transport, so its assistant page reports that the Muse firmware is
required.

## Prerequisites

- A FoloToy AI Passport: ESP32-C3, 8 MB Flash, no PSRAM, three buttons, and a
  USB cable that carries data.
- Windows with PowerShell for the build script. ESP-IDF v6.0.1 with the
  `esp32c3` target; by default the script expects the checkout at
  `..\scratch\esp-idf-v6.0.1` and the installed tools at
  `..\scratch\idf6-tools`.
- A clean checkout of the [Muse Gadget SDK](https://github.com/facebookincubator/muse-gadget-sdk)
  at commit `e6fe5174092702ffddae6f90759a38d55a4ebae1`, by default at
  `..\scratch\muse-gadget-sdk`. The SDK is Apache-2.0 and is not vendored into
  this repository; the build copies and patches it under the ignored `build/`
  directory.
- Python 3 and `pyserial` for the USB tools:
  `python -m pip install pyserial`.
- A Muse SDK token from
  [gadgets.muse.ai SDK tokens](https://gadgets.muse.ai/settings/sdk-tokens),
  after reading the [Gadget SDK Terms](https://gadgets.muse.ai/sdk-terms).
  Every gadget needs a token to pair.
- The Muse app on an iOS or Android phone, signed in, with Developer mode
  enabled under **Settings > Devices**.
- A 2.4 GHz Wi-Fi network. The ESP32-C3 cannot join 5 GHz networks.

## Build the Muse firmware

The firmware is an independent project under `muse/`. The repository root's
default project is still the ESP-IDF 5.5.3 build and is not modified.

### Install ESP-IDF v6.0.1

Install the toolchain once. The build script sets `IDF_TOOLS_PATH` itself, so
install the tools into the default location it uses:

```powershell
New-Item -ItemType Directory -Force ..\scratch | Out-Null
git clone -b v6.0.1 --recursive https://github.com/espressif/esp-idf.git ..\scratch\esp-idf-v6.0.1
$env:IDF_TOOLS_PATH = "$((Resolve-Path ..).Path)\scratch\idf6-tools"
..\scratch\esp-idf-v6.0.1\install.ps1 esp32c3
```

### Fetch the pinned SDK

```powershell
New-Item -ItemType Directory -Force ..\scratch | Out-Null
git clone https://github.com/facebookincubator/muse-gadget-sdk.git ..\scratch\muse-gadget-sdk
git -C ..\scratch\muse-gadget-sdk checkout e6fe5174092702ffddae6f90759a38d55a4ebae1
```

### Build, merge and verify

Run the build script from the repository root in PowerShell:

```powershell
pwsh -File tools\build_passport_muse.ps1
```

The script:

1. exports the ESP-IDF 6.0.1 environment and rejects any other IDF version;
2. copies the pinned SDK's tracked `esp32/` tree to `build/muse-source` and
   applies the Passport patches there, without editing the SDK checkout;
3. refuses a checkout that is not at the pinned commit or has local changes
   under `esp32/`;
4. builds the `muse/` project into `build/muse-firmware`;
5. merges the bootloader, partition table and application into
   `FoloToy-AI-Passport-full.bin`, then runs `tools/verify_firmware.py` to check
   offsets, partition bounds, non-overlap and the 8 MB limit.

Optional arguments:

```powershell
pwsh -File tools\build_passport_muse.ps1 -IdfRoot D:\tools\esp-idf-v6.0.1 -SdkRoot D:\src\muse-gadget-sdk
pwsh -File tools\build_passport_muse.ps1 -PublicAssets
```

- `-IdfRoot` and `-SdkRoot` override the default checkout locations.
- `-PublicAssets` forces the repository's public assets instead of a
  machine-local private asset set configured through
  `private/passport-build.json`.
- The script resolves Windows short paths because the native build tools reject
  paths containing spaces.
- `build/muse-source` is generated. Do not edit it; change the pinned SDK
  checkout or the patch script and rebuild.
- The script builds, verifies and archives the image with its matching ELF/MAP/debug files. It does not flash the badge.

Non-default locations must still satisfy the pinned-commit and clean-checkout
checks. If `build/muse-source` is incomplete, `muse/CMakeLists.txt` stops with a
message asking you to prepare the source first.

Artifacts:

| Path | Contents |
| --- | --- |
| `build/muse-firmware/FoloToy-AI-Passport-full.bin` | Merged image: bootloader, partition table and application. Write it at `0x0`. |
| `build/muse-firmware/FoloToy-AI-Passport.bin` | Application image only. Never write it at `0x0`. |
| `build/muse-firmware/flash_args`, `bootloader/`, `partition_table/` | ESP-IDF build outputs used by the verifier. |

A successful verification prints `Firmware layout: PASS (...)` and
`Merged firmware: PASS (...)`. A failure prints an `ERROR:` line that explains
which image or partition failed; fix it before flashing.

## Flash the firmware

Choose and verify the exact build yourself before writing anything. The merged
image is written from `0x0`; because it also spans the NVS and PHY data regions,
it can reset stored data. The badge's saved Muse pairing, Wi-Fi credentials, SDK
token, assistant notes and results, brightness and profile may be lost.
Segmented flashing through ESP-IDF's `idf.py flash` can preserve NVS when the
partition layout matches, but the build script resolves short paths for the
build only, so prefer the merged image unless you have a verified segmented
workflow.

Example with the esptool bundled in an activated ESP-IDF 6.0.1 environment;
replace the port and adjust the flags to your installed esptool version:

```powershell
python -m esptool --chip esp32c3 --port COM7 --baud 460800 write_flash 0x0 build\muse-firmware\FoloToy-AI-Passport-full.bin
```

Flashing is destructive and needs explicit approval. Do not use `erase-flash`
as a routine step, and never flash the app-only image at `0x0`. See
[firmware layout](../development/engineering/firmware-layout.md#flashing-and-stored-data)
for the repository's flashing and stored-data policy.

## Set the Muse SDK token

The default Muse configuration does not compile a token into the image, so set
it over the USB console after flashing. The token is stored in NVS and used from
the next boot. A stored token takes precedence over any build-time token.

```powershell
python tools\configure_passport_muse.py --port COM7
python tools\configure_passport_muse.py --port COM7 --token-file build\muse-token.txt
```

- Without `--token-file`, the script prompts for the token without echo. Keep
  the token in an ignored file such as `build/muse-token.txt`; never commit it.
- The script sends the line `>sdk_token=...` over USB Serial/JTAG, waits up to
  eight seconds for `@sdk_token ok`, and the device then restarts. A response of
  `@sdk_token error` means the device rejected the token.
- It leaves DTR/RTS unchanged, does not print the token, and requires the Muse
  firmware. On the default 5.5.3 firmware the line is not handled, so the script
  reports that no acknowledgement arrived.
- The device accepts a full 48-character SDK token beginning with `mgst_` and
  containing only base64url characters. The script's own pre-check is looser;
  the device enforces the format, so a truncated or edited token is rejected.
- The UI's USB console is shared with the Passport JSON protocol, so close any
  other serial monitor holding the port before running the command.
- Treat the token as an identifier: it is stored in plain NVS because this
  project disables NVS encryption, Flash encryption and eFuse writes. Revoke a
  leaked token on gadgets.muse.ai, generate a new one, and set it again.
- Muse records the SDK token at pairing time. If the token changes, or the one
  used for pairing is revoked, pair the badge again; the boot log warns when the
  paired token differs from the current one.
- An unpaired badge with no token keeps Bluetooth off, since Muse can't pair
  it without one. The AGENT page asks for the token over USB. Writing it
  restarts the badge, which then turns Bluetooth on for pairing in the Muse
  app. Badges that are already paired are unaffected.

## Pair the badge in the Muse app

1. Power the badge. The AGENT page reports that the device is not paired.
2. In the Muse app open **Settings > Devices**, turn on **Developer mode**, then
   choose **Add Device** (the `+` icon in the top right). The badge advertises as
   `MuseGadget-XXXXXX`.
3. The badge shows the pairing-confirmation state. Press OK once to confirm that
   the pairing request is yours. The press is consumed by pairing and does not
   start a recording.
4. The badge joins Wi-Fi and connects; the state moves from connecting to ready.
   Configure Wi-Fi for the gadget in the Muse app if it stays in connecting or
   offline.
5. Pairing is required before Muse can call the device tools.

Pairing requires that explicit physical press and creates a fresh encrypted
session. The community SDK has no manufacturer verification, so it cannot
prevent an active man-in-the-middle attack; set the gadget up on a network you
trust.

To change networks or recover an offline paired badge, hold OK on the badge,
select **Wi-Fi config**, then **Re-pair Muse**. A separate confirmation defaults
to Cancel. Confirming clears only the SDK's Muse setup and saved networks, then
restarts the badge for phone pairing. Passport profile, media, notes and task
results remain intact. This also supports pairing again after a token change;
no NVS or full-Flash erase is part of this recovery flow.

Pairing consent is bound to the SDK session displayed when OK is pressed. A
request arriving during an ordinary speech click, or replacing an earlier
request, cannot consume that click as consent.

## Use the assistant

Enter the assistant by selecting the AGENT button on the badge and pressing OK.
The AGENT page shows one state at a time:

| State | Meaning |
| --- | --- |
| Unavailable | This firmware has no Muse transport. |
| Unpaired | The badge is advertising; add it in the Muse app. |
| Pairing confirmation | A pairing request is waiting; press OK to confirm. |
| Connecting | Pairing is complete; Wi-Fi or the session is not ready. |
| Ready | A turn can start. |
| Offline | The link is down or reported an error. |
| Recording | Microphone audio is being sent. |
| Waiting | The turn was submitted; Muse is processing it. |
| Reply | A reply is displayed. |
| Error | The last action failed; OK starts a new attempt. |

Controls on the assistant page:

- OK starts a recording when the state is ready; pressing OK again sends it.
- UP/DOWN scroll the reply. When the reply is longer than the view, the badge
  scrolls the text instead of changing pages.
- Holding OK for two seconds leaves the assistant. Leaving cancels a recording
  that has not been sent; a turn already submitted continues on Muse and its
  result can be read from the task-results page later.
- If the connection drops during a recording or while waiting for a reply, the
  local turn is cancelled and the page reports that the connection was lost.
- Recordings are limited to 30 seconds. Reaching the limit cancels the recording
  and asks for a retry; the audio is sent as 16 kHz, 16-bit mono.
- If OK is pressed while a reply is still being processed, the badge asks you
  to wait instead of starting a second turn.

### The hidden menu

Hold OK for two seconds on the badge, regardless of which bottom button is
highlighted, to open the menu. On any other page the same hold returns to the
badge. Menu rows, in order:

1. **Display brightness** — UP/DOWN step from 10% to 100% in 10% steps and save the value.
2. **About** — product, version and board.
3. **Wi-Fi config** — see below.
4. **Game** — the existing game page.
5. **Task results** — the last completed assistant reply.
6. **Local notes** — the newest eight notes, one per screen.
7. **Return** — back to the badge.

The task-results page shows the last completed reply. OK saves the shown text as
a local note when it fits the note limit; results longer than 255 bytes cannot be
saved that way. The notes page switches notes with UP/DOWN and shows the current
position; saving a ninth note discards the oldest one.

### Wi-Fi configuration on the Muse firmware

On the Muse firmware this row opens **Network and Muse**, showing the device
name, network, address and SDK-token availability. It remains reachable offline
and provides the physical re-pairing recovery above.

Choose **Open profile configuration** while connected to start the existing
profile/media editor on the station network for five minutes. The badge keeps
the URL, username `passport` and temporary 12-hex-digit access code visible on
this screen. Open that URL on the same LAN and enter those credentials into the
browser's login prompt. Every registered route and the fallback route require
authorization. Requests with another Host or Origin are refused. The code is
new for each session, is not logged, and stops working when the server closes.
HTTP Basic authorization runs over local HTTP; use a trusted LAN.

Without a network (no SDK token yet, not paired, or out of range) the same
choice falls back to the badge's own open hotspot `Badge-XXXX`, the captive
portal described in the [user guide](passport-os-v0.1-user-guide.md). The
screen shows the hotspot name and `192.168.4.1`. The firmware adds the access
point beside the station interface the SDK already runs, then returns to
station-only mode after five minutes; BLE pairing and the SDK's Wi-Fi stay
untouched. If the badge was paired but its network is out of range, the station
keeps retrying in the background and can briefly move the hotspot channel.

After the badge first gets an address it starts SNTP with `ntp.aliyun.com`,
`cn.pool.ntp.org` and `pool.ntp.org`. The clock becomes valid on the first
successful sync and lwIP renews it hourly. A USB `time` command or the
configuration page can still set it before then.

On the default 5.5.3 firmware the same menu row retains the hotspot and captive
portal described in the [user guide](passport-os-v0.1-user-guide.md).

## Device tools

When the encrypted control session registers, the firmware adds exactly four
Passport device tools to the registration Muse receives. Muse can call them with
JSON parameters; the firmware validates every parameter and dispatches accepted
work to the application's existing owners. The SDK's own standard Link commands
(for example `device.health`) remain registered by the SDK. None of the
Passport tools exposes a shell, profile fields, credentials or files.

Calls that queue work return this acknowledgement:

```json
{"ok": true, "payload": {"accepted": true, "status": "queued; not yet applied or persisted"}}
```

`passport.status` returns its fields directly in `payload` instead. Every
failure uses this shape:

```json
{"ok": false, "error": {"code": "passport_error", "message": "..."}}
```

| Tool | Parameters | Behavior |
| --- | --- | --- |
| `passport.status` | none | Read the current device status. |
| `passport.brightness` | `percent`: number, a multiple of 10 from 10 to 100 | Queue a brightness change. |
| `passport.note` | `text`: non-empty UTF-8 string, at most 255 bytes | Queue saving a local note. |
| `passport.open` | `page`: one of `badge`, `agent`, `qr`, `menu`, `tasks`, `notes` | Queue opening a device page. |

### `passport.status`

This tool is read-only and returns the applied state:

- `battery_percent`: number, or `null` when the battery reading is unavailable.
- `brightness_percent`: the applied brightness, a multiple of 10 from 10 to 100.
- `page`: one of `badge`, `qr`, `menu`, `brightness`, `about`, `game`, `agent`,
  `tasks`, `notes`.

Poll this tool after a queued brightness or page change if you need to confirm
that it was applied. A battery value of `null` means the gauge is missing or
returned an invalid reading; it does not mean zero.

### `passport.brightness`

- `percent` must be a JSON number that is a multiple of 10 from 10 to 100. Decimal
  values, strings, `null` and other numbers are rejected with
  `percent must be a multiple of 10 from 10 to 100`.
- If the device input owner is stopped or its queue is full, the call fails with
  `Device is busy or stopping; command not accepted`.
- A successful call only queues the change. The UI applies it and the
  single-writer storage path saves it afterwards; the response does not mean the
  screen has already changed or that the value is in Flash.

### `passport.note`

- `text` must be a non-empty JSON string of valid UTF-8 and at most 255 bytes.
  Empty strings, missing or non-string values, invalid UTF-8 and longer text are
  rejected with `text must be nonempty UTF-8 of at most 255 bytes`.
- The device keeps the newest eight notes. Each new note becomes the newest one;
  when all eight slots are full the oldest note is discarded.
- If the assistant worker is stopping or its job queue is full, the call fails
  with `Note queue is busy; note not accepted`.
- A successful call only means the note job was queued. The assistant worker
  saves the note and persists the journal; a storage failure is reported on the
  device screen, not in this response.

### `passport.open`

- `page` is required and must be exactly one of `badge`, `agent`, `qr`, `menu`,
  `tasks`, `notes`. Any other value, including a missing one, fails with
  `Unknown or missing page`.
- `brightness`, `about` and `game` are valid values for `passport.status` but are
  not accepted by `passport.open`.
- If the input owner is stopped or its queue is full, the call fails with
  `Device navigation is busy`.
- Opening a page does not cancel an already submitted cloud task. Leaving the
  assistant page only cancels a recording that has not been sent.

### Queued is not applied or persisted

The success payload deliberately says `queued; not yet applied or persisted`.
For brightness, notes and page changes the tool has only placed work on an
in-device queue:

- The UI owner applies brightness and navigation on its next input cycle.
- The assistant worker applies notes and persists completed replies in its own
  NVS write.
- A restart, power loss, crash or failed NVS write between acceptance and
  persistence loses the change even though the tool returned success.
- Use `passport.status` to read the applied brightness and page. There is no tool
  that reads back saved notes or journal contents.

## Persistence and stored data

- Assistant data lives in the NVS namespace `passport_agent`, key `journal`. The
  blob contains a version, the note count, the last completed task result (up to
  1023 bytes) and the newest eight notes (up to 255 bytes each); it is about
  3 KB.
- It is written when a reply completes and when a note is saved, and loaded at
  boot. Restoring validates the version and UTF-8 content; an incompatible or
  corrupt blob is ignored and the journal starts empty.
- The journal survives a reboot and application flashing that leaves NVS intact.
  Writing a merged image from `0x0` can overwrite NVS and lose it.
- Profile fields, the picture and avatar data, brightness and USB settings stay
  in the existing Passport records and Flash partitions described in the user
  guide. Muse pairing credentials, Wi-Fi credentials and the SDK token live in
  the SDK's NVS records.
- This project disables NVS encryption, Flash encryption and eFuse writes, so
  anyone with physical access can read the stored Muse token and Wi-Fi
  credentials. Do not add personal secrets to the notes.

## Capability boundaries

Provided by this integration:

- Community Bluetooth LE pairing through the Muse app, with an explicit physical
  confirmation press.
- An encrypted control session and streamed voice notes to Muse.
- Text replies on the badge with scrolling, a last-result page and eight
  persistent local notes.
- Four validated device tools for status, brightness, local notes and page
  navigation.
- The profile, picture, avatar and game features of AI PassportOS, and a
  Wi-Fi configuration page served over the network Muse already joined, with a
  hotspot fallback while offline.

Not provided by this integration:

- **No bundled cloud TTS provider.** Muse replies are text; this firmware adds
  bounded speech playback through an independently configured endpoint. The
  optional Windows bridge needs a running PC. See [speech service options](passport-agent-voice.md).
- **No complete long-reply or history synchronization.** The device keeps a
  bounded reply (about 1 KB) for display and always labels it as a preview,
  directing the user to the phone for the complete answer. Longer answers, earlier turns, account tasks and Muse history are only in
  the Muse app; no tool or page imports them.
- **No cloud rollback.** Cancelling locally, leaving the assistant or pressing a
  button does not undo anything Muse already did. A submitted voice note may
  already have triggered a cloud action, and a queued device command is not a
  transaction.
- **No automatic resend or offline queue.** A failed turn is not replayed after
  reconnection; the user must retry. Voice notes are not buffered while
  offline.
- **No OTA, video, Hatch session, Flash encryption or eFuse features.** The
  build disables them; the badge is updated over USB.
- **No arbitrary remote control.** The four Passport tools above are the only
  additions; the SDK's own standard Link commands are unchanged. The Passport
  tools cannot write the profile, credentials or token, cannot run commands, and
  cannot open the brightness, about or game pages.
- **No Muse transport in the default firmware.** The ESP-IDF 5.5.3 project still
  builds, but its assistant reports the Muse firmware requirement and has no
  device tools.

Unverified here:

- The Muse firmware build, host tests and device behavior are reported
  separately; this page does not record a passing build.
- Pairing, live Muse turns, reply latency, RAM headroom and peaks, microphone and
  speaker quality, Wi-Fi coexistence, idle-screen current, and the re-pairing
  recovery path require on-device testing. Host tests are not hardware acceptance.

## Troubleshooting

| Symptom | What to check |
| --- | --- |
| `Invalid SDK token format` from the script | The token must be a complete 48-character `mgst_...` token with no whitespace; copy it again from gadgets.muse.ai. |
| `Device rejected SDK token` | The token is malformed, revoked or expired. Generate a new token and set it again. |
| `No token acknowledgement` | The Muse firmware may not be installed, the port is wrong, or another terminal holds the port. Close `idf.py monitor` and retry. |
| The badge never appears in the Muse app | Set the token first, enable Developer mode, restart the badge, and look for `MuseGadget-XXXXXX`. Remove a stale entry in the app before adding again. |
| The assistant page reports the Muse firmware is required | The badge runs the default 5.5.3 firmware. Build and flash the Muse firmware. |
| The page asks to pair in the phone app | Add the badge in the Muse app, then press OK when the pairing-confirmation state appears. |
| Connecting or offline does not clear | Open Wi-Fi config on the badge and use Re-pair Muse to recover an offline paired gadget; then configure a 2.4 GHz network in the phone app. |
| Pressing OK only asks you to wait | A turn is still being processed. Wait for the reply or leave the page; do not retry repeatedly. |
| A reply looks cut off | The device keeps a bounded reply. Read the full answer in the Muse app; the remaining text is not synchronized. |
| A saved note is rejected | The text is empty, invalid UTF-8 or longer than 255 bytes. Save a shorter summary. |
| Notes and results disappear after flashing | A merged image written from `0x0` can reset NVS. Keep NVS intact by using a verified segmented flash, and export data before a full refresh. |
| A tool answered `queued...` but nothing changed | The request was only queued, the device may be busy, or storage failed later. Poll `passport.status` and check the badge screen. |
| The Muse build fails | Confirm ESP-IDF 6.0.1, a clean SDK checkout at the pinned commit, and the default or correctly passed `-IdfRoot`/`-SdkRoot`. Delete `build/muse-source` and `build/muse-firmware` to rebuild from scratch. |

## Related documents

- [AI PassportOS v0.1 user guide](passport-os-v0.1-user-guide.md) for the
  controls, profile, picture and USB protocol of the shared application.
- [Firmware layout](../development/engineering/firmware-layout.md) for
  partition addresses, merged images and stored-data policy.
- [Build and test](../development/engineering/build-and-test.md) for the
  default firmware validation commands.
