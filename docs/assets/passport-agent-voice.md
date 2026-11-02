[简体中文](passport-agent-voice.zh_CN.md) · **English**

# Agent animation and reply speech

The Agent page uses an independent Muse character: Nora in the personal build,
based on the user's 2026-10-24 reference. The identity card keeps its own avatar.
Nora has eight 128×132 native frames for idle/blink, listening, thinking, two speaking
mouths and completion. Speaking mouths respond to actual PCM level and close
during silence; this is amplitude feedback, not phoneme lip synchronization.
Generate the private LCD pack with:

```text
python tools/build_passport_agent_avatar.py --sheet assets/private/muse-nora/nora-states-v1.png --output build/passport-assets
```

Listening and speech animation follow actual PCM levels. The identity card's
uploaded image is unchanged. Reply scrolling and character objects survive level
updates. As requested on 2026-10-28, Agent uses a plain white full-screen canvas:
Nora's name, portrait, subtitles and necessary prompts, with no clock, battery,
card, meters or long-return reminder. Badge buttons are 44 px tall, name and
organization use 24 px bold type, and research text uses 20 px. Menu navigation
and physical long-OK actions are retained.

OK starts recording, the next OK sends. During speech preparation or playback,
OK stops local speech; another OK starts a new recording. Leaving the Agent page
stops local playback and suppresses autoplay for that pending turn, even if you
return before it completes. Submitted cloud tasks continue. Failed speech keeps
the text. The screen labels unconfigured speech explicitly.

## Speech service

Muse supplies reply text. This firmware adds an independent TTS output path;
it does not assume a paid provider or reuse Muse credentials for TTS.

- A configured HTTPS endpoint lets the device work without a PC, provided that
  endpoint implements the protocol below. No cloud provider is preselected.
- The optional Windows bridge uses an installed Chinese Windows voice and needs
  the PC running on the same network. It is not a standalone cloud service.

### Optional Windows bridge

Install Python with `pyserial` for USB configuration. The bridge itself uses only
Python's standard library and Windows System.Speech. Check a voice without
starting a network service:

```text
python tools/passport_voice_bridge.py --sample build/chinese-speech.wav
```

To accept your device's connection, replace the example address with the PC's
private IPv4 address. Keep the process running; allow its port only on the
intended local network if Windows prompts for firewall access.

```text
python tools/passport_voice_bridge.py --host 0.0.0.0 --device-host 192.168.1.20
python tools/configure_passport_voice.py --port COM7 --config build/passport-voice.local.json
```

The first command creates a random token in the ignored local JSON file and
never prints it. The second sends the URL/token to installed voice-capable
firmware without changing DTR/RTS or rebooting it. Use the actual device port.
Speech credentials are stored under NVS `passport_voice/config`; there is no
Flash/NVS encryption. The bridge is authenticated but uses HTTP on a private
network. Use HTTPS for remote services. This helper's PowerShell execution-policy
setting applies only to its child process, not machine/user policy.

```text
python tools/configure_passport_voice.py --port COM7 --disable
```

This disables speech without touching Muse pairing, Wi-Fi or Passport records.

### Endpoint protocol

POST the UTF-8 reply as `text/plain; charset=utf-8`, with
`Authorization: Bearer <private token>`. The endpoint must return HTTP 200,
`X-Passport-Audio: pcm_s16le_16000_mono`, and raw PCM16 little-endian mono at
16,000 Hz. Fixed-length and chunked responses are accepted. Redirects are
rejected. Other vendor API formats need an adapter; a generic vendor API URL
alone is not sufficient.

Text is limited to the device's 1,023-byte reply preview. Speech is bounded to
120 seconds, uses small device buffers and never holds the whole audio file in
device RAM. No data for ten seconds or a turn exceeding 150 seconds ends speech.
An independent socket guard aborts cancellation, slow headers, idle response data
and the absolute deadline even when the HTTP client internally keeps reading.
It is joined before client cleanup and never owns audio. Successful playback
flushes bounded silence through the BSP DMA ring before sleep, preserving the
last audible samples; cancellation skips that drain. DNS/TLS timing and peak
memory under simultaneous Muse traffic still need device tests.

## Verification boundary

Host tests cover PCM boundaries, output/time limits, HTTP response format,
authentication, premature EOF, cancel/leave/reenter, text preservation and stable
UI objects/timers. LVGL renders are real application output. A synthesized WAV
sample verifies Windows speech generation, not Passport's speaker or network.
Device audio, latency, simultaneous Muse/TTS memory and acoustic quality remain
on-device acceptance checks. Building or configuring a service does not authorize
flashing, global firewall changes, a paid service, or publishing credentials.
