#!/usr/bin/env python3
"""Local web service to edit the Passport profile, pick an avatar, and re-render the badge.

Run with the host venv Python:
    build/host-venv/Scripts/python.exe tools/passport_web.py
Then open http://127.0.0.1:8788 . The service binds localhost only.
"""
from __future__ import annotations
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import threading
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from configure_passport import normalize_profile  # noqa: E402

FONT = Path(r"C:\Espressif\asset-tools\fonts\NotoSansSC-variable.ttf")
FONT_CONV = Path(r"C:\Espressif\asset-tools\node_modules\lv_font_conv\lv_font_conv.js")
NODE = Path(r"C:\Program Files\nodejs\node.exe")
CMAKE = Path(r"C:\Espressif\tools-v5.5.3\tools\cmake\3.30.2\bin\cmake.exe")
TOOL_PATH_PREFIX = r"C:\Espressif\tools-v5.5.3\tools\cmake\3.30.2\bin;" \
                   r"C:\Espressif\tools-v5.5.3\tools\ninja\1.12.1"

PROFILE_JSON = ROOT / "build" / "profile.local.json"
AVATAR_CURRENT = ROOT / "build" / "avatar-current.png"
ASSETS_DIR = ROOT / "build" / "passport-assets"
PREVIEW_BUILD = ROOT / "build" / "preview-host"
PREVIEW_OUT = ROOT / "build" / "preview-private"
PACK = ROOT / "build" / "avatar-pack"

REGROUP = threading.Lock()
REGROUP_STATE = {"busy": False, "error": "", "done_at": ""}


def _tool_env() -> dict:
    env = os.environ.copy()
    env["PATH"] = TOOL_PATH_PREFIX + env.get("PATH", "")
    return env


def _run(command: list[str], *, env: dict | None = None, timeout: int = 600) -> None:
    try:
        subprocess.run(command, check=True, capture_output=True, text=True,
                       cwd=ROOT, env=env, timeout=timeout)
    except FileNotFoundError as error:
        raise RuntimeError("无法启动 " + command[0]) from error


def regenerate(avatar: Path | None) -> None:
    """Assets -> preview binary -> rendered PNGs. Raises with the failing step."""
    command = [sys.executable, str(ROOT / "tools" / "build_passport_assets.py"),
               "--font", str(FONT), "--font-conv", str(FONT_CONV), "--node", str(NODE),
               "--output", str(ASSETS_DIR), "--profile", str(PROFILE_JSON)]
    if avatar is not None:
        command += ["--avatar", str(avatar)]
    if (PACK/"manifest.json").is_file():
        command += ["--gallery",str(PACK)]
    _run(command)
    _run([str(CMAKE), "--build", str(PREVIEW_BUILD), "-j", "8"], env=_tool_env())
    _run([str(PREVIEW_BUILD / "passport_preview.exe"), str(PREVIEW_OUT),
          str(PREVIEW_OUT / "profile.local.txt")])
    from PIL import Image
    for raw in PREVIEW_OUT.glob("*.rgb565"):
        Image.frombytes("RGB", (240, 320), raw.read_bytes(), "raw", "BGR;16").save(raw.with_suffix(".png"))


def write_fixture(profile: dict) -> None:
    PREVIEW_OUT.mkdir(parents=True, exist_ok=True)
    fixture = PREVIEW_OUT / "profile.local.txt"
    fixture.write_text("\n".join(profile[k].replace("\n", " ")
                                 for k in ("name", "title", "research", "bio")) + "\n",
                       encoding="utf-8")


def load_profile() -> dict:
    try:
        data = json.loads(PROFILE_JSON.read_text(encoding="utf-8-sig"))
    except (OSError, ValueError):
        data = {}
    return {key: data.get(key, "") for key in ("name", "title", "research", "bio", "homepage")}


def load_scenes() -> list[dict]:
    try:
        manifest = json.loads((PACK / "manifest.json").read_text(encoding="utf-8"))
        return manifest.get("scenes", [])
    except (OSError, ValueError):
        return []


def parse_multipart(body: bytes, boundary: bytes) -> dict[str, tuple[str, bytes]]:
    """Return {field: (filename or "", data)} from a multipart/form-data body."""
    result: dict[str, tuple[str, bytes]] = {}
    for chunk in body.split(b"--" + boundary):
        chunk = chunk.strip(b"\r\n")
        if not chunk or chunk == b"--":
            continue
        header_blob, _, data = chunk.partition(b"\r\n\r\n")
        headers = {}
        for line in header_blob.split(b"\r\n"):
            key, _, value = line.decode("latin-1").partition(":")
            headers[key.strip().lower()] = value.strip()
        disposition = headers.get("content-disposition", "")
        name_match = re.search(r'name="([^"]*)"', disposition)
        file_match = re.search(r'filename="([^"]*)"', disposition)
        if name_match:
            result[name_match.group(1)] = (file_match.group(1) if file_match else "", data)
    return result


PAGE = """<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8">
<title>Passport 工牌配置</title>
<style>
 :root { color-scheme: dark; }
 body { font-family: "Segoe UI", "Microsoft YaHei", sans-serif; background: #0a1c33;
        color: #f4faff; margin: 0; padding: 24px; }
 h1 { font-size: 20px; color: #3dff9e; margin: 0 0 4px; }
 .sub { color: #8fa3c0; font-size: 13px; margin-bottom: 20px; }
 .cols { display: flex; gap: 28px; flex-wrap: wrap; align-items: flex-start; }
 form { background: #0c1d3a; border: 1px solid #1a4a6e; border-radius: 12px; padding: 18px; width: 340px; }
 label { display: block; font-size: 13px; color: #8fa3c0; margin: 12px 0 4px; }
 input[type=text], textarea, select { width: 100%; box-sizing: border-box; background: #081428;
        border: 1px solid #1a4a6e; border-radius: 6px; color: #f4faff; padding: 8px; font-size: 14px; }
 input[type=file] { color: #8fa3c0; font-size: 13px; margin-top: 4px; width: 100%; }
 button { margin-top: 18px; width: 100%; padding: 12px; font-size: 15px; border-radius: 8px;
          border: 2px solid #3dff9e; outline: 2px solid rgba(61,255,158,.3); background: #0c1d3a;
          color: #3dff9e; cursor: pointer; }
 button:disabled { opacity: .5; cursor: wait; }
 .status { font-size: 13px; margin-top: 10px; color: #8fa3c0; min-height: 18px; }
 .err { color: #ff7a7a; }
 .ok { color: #3dff9e; }
 .preview img { image-rendering: pixelated; width: 240px; border-radius: 10px;
                border: 1px solid #1a4a6e; display: block; }
 .preview .cap { font-size: 12px; color: #8fa3c0; margin-top: 6px; }
 .thumb { margin-top: 8px; }
 .thumb img { image-rendering: pixelated; width: 86px; height: auto;
              border: 1px solid #1a4a6e; border-radius: 8px; background: #081428; }
 .hint { font-size: 12px; color: #8fa3c0; }
</style></head><body>
<h1>Passport 工牌配置</h1>
<div class="sub">修改资料 / 选择头像 → 保存并重新渲染预览（约 20 秒）。烧录仍需在终端执行。</div>
<div class="cols">
 <div class="preview">
  <img id="p0" src="/preview/page-0.png" alt="badge">
  <div class="cap">身份卡预览</div>
  <div class="thumb"><img id="scene-thumb" src="/scene/01_default.png" alt="scene"></div>
  <div class="cap hint">当前头像场景预览（选择下拉框时更新）</div>
 </div>
 <form id="f">
  <label>姓名</label><input type="text" name="name" value="@NAME@" maxlength="64">
  <label>职位 / 身份</label><input type="text" name="title" value="@TITLE@" maxlength="96">
  <label>单位 / 标签</label><input type="text" name="research" value="@RESEARCH@" maxlength="192">
  <label>简介</label><textarea name="bio" rows="2" maxlength="512">@BIO@</textarea>
  <label>个人主页链接</label>
  <input type="text" name="homepage" value="@HOMEPAGE@" maxlength="128" placeholder="https://...">
  <div class="hint">保存资料不会写入背面 NFC。碰一碰分享需用手机 NFC 读写 App，另将同一网址写为 URL/URI 记录。</div>
  <label>头像：场景选择</label>
  <select id="scene" name="scene">@OPTIONS@</select>
  <label>头像：或上传自定义图片（PNG/JPG，全身竖图最佳）</label>
  <input type="file" name="avatar" accept="image/png,image/jpeg">
  <div class="hint">上传文件优先于场景选择；两者留空则保留当前头像。</div>
  <button id="b" type="submit">保存并重新渲染</button>
  <div class="status" id="s"></div>
 </form>
</div>
<script>
const sel = document.getElementById('scene');
sel.addEventListener('change', () => {
  document.getElementById('scene-thumb').src = '/scene/' + sel.value + '.png';
});
document.getElementById('f').addEventListener('submit', async (e) => {
  e.preventDefault();
  const button = document.getElementById('b'), status = document.getElementById('s');
  button.disabled = true; status.textContent = '正在生成资产并渲染…'; status.className = 'status';
  try {
    const reply = await fetch('/save', { method: 'POST', body: new FormData(e.target) });
    const data = await reply.json();
    if (!data.ok) throw new Error(data.error || '保存失败');
    status.textContent = '已更新 ' + data.done_at; status.className = 'status ok';
    document.getElementById('p0').src = '/preview/page-0.png?t=' + Date.now();
  } catch (error) {
    status.textContent = String(error.message || error); status.className = 'status err';
  } finally { button.disabled = false; }
});
</script></body></html>
"""


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt: str, *args) -> None:
        pass  # keep the console quiet; errors still print via status state

    # -- helpers -------------------------------------------------------
    def _send(self, code: int, body: bytes, ctype: str) -> None:
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def _png(self, path: Path) -> None:
        if path.is_file():
            self._send(200, path.read_bytes(), "image/png")
        else:
            self._send(404, b"not found", "text/plain")

    # -- GET -----------------------------------------------------------
    def do_GET(self) -> None:
        route = urlparse(self.path).path
        if route == "/":
            profile = load_profile()
            options = ['<option value="">— 保留当前头像 —</option>']
            for scene in load_scenes():
                sid = scene.get("id", "")
                title = scene.get("title_zh", sid)
                options.append(f'<option value="{sid}">{title}（{sid}）</option>')
            def esc(value: str) -> str:
                return (value.replace("&", "&amp;").replace("<", "&lt;")
                        .replace(">", "&gt;").replace('"', "&quot;"))
            html = (PAGE
                    .replace("@NAME@", esc(profile["name"]))
                    .replace("@TITLE@", esc(profile["title"]))
                    .replace("@RESEARCH@", esc(profile["research"]))
                    .replace("@BIO@", esc(profile["bio"]))
                    .replace("@HOMEPAGE@", esc(profile["homepage"]))
                    .replace("@OPTIONS@", "".join(options)))
            self._send(200, html.encode("utf-8"), "text/html; charset=utf-8")
        elif route.startswith("/preview/"):
            name = Path(route).name
            if re.fullmatch(r"[A-Za-z0-9._-]+", name):
                self._png(PREVIEW_OUT / name)
            else:
                self._send(400, b"bad name", "text/plain")
        elif route.startswith("/scene/"):
            name = Path(route).name
            if re.fullmatch(r"[A-Za-z0-9._-]+", name):
                self._png(PACK / "png" / name)
            else:
                self._send(400, b"bad name", "text/plain")
        else:
            self._send(404, b"not found", "text/plain")

    # -- POST ----------------------------------------------------------
    def do_POST(self) -> None:
        if urlparse(self.path).path != "/save":
            self._send(404, b'{"ok":false,"error":"not found"}', "application/json")
            return
        ctype = self.headers.get("Content-Type", "")
        if "multipart/form-data" not in ctype or "boundary=" not in ctype:
            self._send(400, b'{"ok":false,"error":"expected multipart form"}', "application/json")
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            self._send(400, b'{"ok":false,"error":"bad length"}', "application/json")
            return
        boundary = ctype.split("boundary=", 1)[1].strip().encode("latin-1")
        parts = parse_multipart(self.rfile.read(length), boundary)

        profile = {key: parts.get(key, ("", b""))[1].decode("utf-8", "replace").strip()
                   for key in ("name", "title", "research", "bio", "homepage")}
        try:
            profile = normalize_profile(profile)
        except ValueError as error:
            self._send(400, json.dumps({"ok": False, "error": str(error)}).encode(), "application/json")
            return

        avatar: Path | None = None
        filename, upload = parts.get("avatar", ("", b""))
        scene = parts.get("scene", ("", b""))[1].decode("ascii", "replace").strip()
        if filename and upload:
            AVATAR_CURRENT.parent.mkdir(parents=True, exist_ok=True)
            AVATAR_CURRENT.write_bytes(upload)
            avatar = AVATAR_CURRENT
        elif scene:
            source = PACK / "png" / (scene + ".png")
            if not source.is_file():
                self._send(400, json.dumps({"ok": False, "error": "unknown scene"}).encode(),
                           "application/json")
                return
            AVATAR_CURRENT.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, AVATAR_CURRENT)
            avatar = AVATAR_CURRENT
        elif AVATAR_CURRENT.is_file():
            avatar = AVATAR_CURRENT

        PROFILE_JSON.parent.mkdir(parents=True, exist_ok=True)
        PROFILE_JSON.write_text(json.dumps(profile, ensure_ascii=False, indent=2) + "\n",
                                encoding="utf-8")
        write_fixture(profile)

        if not REGROUP.acquire(blocking=False):
            self._send(409, b'{"ok":false,"error":"another regeneration is running"}',
                       "application/json")
            return
        REGROUP_STATE.update(busy=True, error="")
        try:
            regenerate(avatar)
            from datetime import datetime
            REGROUP_STATE["done_at"] = datetime.now().strftime("%H:%M:%S")
            payload = {"ok": True, "done_at": REGROUP_STATE["done_at"]}
        except subprocess.CalledProcessError as error:
            detail = (error.stderr or error.stdout or str(error)).strip().splitlines()
            REGROUP_STATE["error"] = detail[-1] if detail else str(error)
            payload = {"ok": False, "error": "管线失败: " + REGROUP_STATE["error"][:200]}
        except Exception as error:  # noqa: BLE001 - surfaced to the browser
            REGROUP_STATE["error"] = str(error)
            payload = {"ok": False, "error": str(error)[:200]}
        finally:
            REGROUP_STATE["busy"] = False
            REGROUP.release()
        self._send(200 if payload["ok"] else 500,
                   json.dumps(payload).encode("utf-8"), "application/json")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8788)
    parser.add_argument("--no-browser", action="store_true")
    args = parser.parse_args()
    if not FONT.is_file():
        parser.error(f"Font not found: {FONT}")
    server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    url = f"http://127.0.0.1:{args.port}"
    print(f"Passport web configuration service on {url} (Ctrl+C to stop)")
    if not args.no_browser:
        webbrowser.open(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("stopped")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
