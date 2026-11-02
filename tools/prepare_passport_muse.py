#!/usr/bin/env python3
"""Prepare an isolated, pinned Muse SDK integration without editing its checkout."""
from pathlib import Path
import argparse
import os
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[1]
MUSE_REV="e6fe5174092702ffddae6f90759a38d55a4ebae1"

def replace_once(source,old,new):
    if source.count(old)!=1:
        raise ValueError("Pinned SDK patch anchor missing or ambiguous: "+old[:72])
    return source.replace(old,new,1)

def validate_output(path):
    build=Path(os.path.abspath(ROOT/"build"))
    candidate=Path(os.path.abspath(path))
    message="Generated SDK must stay under this project's ignored build directory"
    if not candidate.is_relative_to(build) or candidate==build:
        raise ValueError(message)
    if not candidate.resolve().is_relative_to(build.resolve()):
        # build/ subdirectories may be junctions to faster local storage (Windows
        # native tools reject spaces in paths, so the SDK often lives on another
        # volume). Allow only that explicit redirect; other escapes still fail.
        if not os.path.isjunction(candidate) and not os.path.islink(candidate):
            raise ValueError(message)

def prepare(source,output):
    source=Path(source).resolve();output=Path(output);validate_output(output);output=output.resolve()
    rev=subprocess.check_output(["git","-C",str(source),"rev-parse","HEAD"],text=True).strip()
    if rev!=MUSE_REV: raise ValueError("Muse source must be pinned to "+MUSE_REV)
    # Refuse local edits to executable integration sources; never copy credentials.
    changed=subprocess.check_output(["git","-C",str(source),"status","--porcelain","--untracked-files=no","--","esp32"],text=True)
    if changed.strip(): raise ValueError("Muse SDK checkout has local changes; use a clean pinned checkout")
    tracked=subprocess.check_output(["git","-C",str(source),"ls-files","-z","--","esp32"],text=True).split('\0')
    for name in filter(None,tracked):
        relative=Path(name).relative_to("esp32");target=output/relative;original=source/name
        target.parent.mkdir(parents=True,exist_ok=True)
        if not target.exists() or target.stat().st_mtime_ns!=original.stat().st_mtime_ns:
            shutil.copy2(original,target)
    # Only tracked vendor files cross; private avatar overrides must not cross.
    private_avatar=output/"components/muse/avatar/muse_pixel.c"
    if private_avatar.exists(): raise ValueError("Private avatar override found in generated SDK")
    cmake=output/"components/muse/CMakeLists.txt"
    cmake.write_text(replace_once(cmake.read_text(encoding="utf-8"), '        "muse_app.c"\n', ''),encoding="utf-8")
    settings=output/"components/muse/muse_settings.c"
    s=settings.read_text(encoding="utf-8")
    s=replace_once(s,'        ESP_LOGW(TAG, "NVS layout changed, erasing");\n        ESP_ERROR_CHECK(nvs_flash_erase());\n        err = nvs_flash_init();',
        '        ESP_LOGE(TAG, "NVS needs explicit recovery; preserving user data");\n        return err;')
    settings.write_text(s,encoding="utf-8")
    manifest=output/"components/muse/idf_component.yml"
    manifest.write_text(replace_once(manifest.read_text(encoding="utf-8"),'version: "~1.5"','version: "1.6.2"'),encoding="utf-8")
    app=output/"main/app.c";s=app.read_text(encoding="utf-8")
    s=replace_once(s,'#include "app.h"','#include "app.h"\n#include "passport_muse_tools.h"')
    anchor='static cJSON *on_ws_command(\n    const char *command, cJSON *params, const char *request_id,\n    noise_ctrl_session_generation_t session_generation) {'
    s=replace_once(s,anchor,anchor+'\n    cJSON *passport_result = passport_muse_command(command, params);\n    if (passport_result) return passport_result;')
    app.write_text(s,encoding="utf-8")
    with app.open("a",encoding="utf-8") as f:
        f.write('''\n/* Passport binds physical consent to the displayed SDK pairing session. */
uint32_t passport_pairing_confirm_session(uint32_t expected);
uint32_t passport_muse_pending_session(void) {
    return link_pairing_confirmation_required() ? link_pairing_session_generation() : 0;
}
bool passport_muse_confirm_pairing(uint32_t expected) {
    if (!expected || !ble_server_has_connection()) return false;
    uint32_t confirmed = passport_pairing_confirm_session(expected);
    if (confirmed) notify_pairing_confirmed(confirmed);
    return confirmed != 0;
}
''')
    pairing=output/"main/link_pairing.c"
    with pairing.open("a",encoding="utf-8") as f:
        f.write('''\n/* Atomic expected-session variant of link_pairing_confirm_active_session. */
uint32_t passport_pairing_confirm_session(uint32_t expected) {
    lock_take();
    bool ok = expected && expected == s_generation && !expired_locked()
              && s_state == PAIRING_CONFIRM_REQUIRED && s_confirmation_armed;
    if (ok) {
        advance_generation_locked(); s_state = PAIRING_READY;
        s_deadline_us = esp_timer_get_time() + PAIRING_TIMEOUT_US;
    }
    uint32_t generation = ok ? s_generation : 0;
    lock_give(); return generation;
}
''')
    noise=output/"main/noise_control.cpp";s=noise.read_text(encoding="utf-8")
    s=replace_once(s,'static char *build_register_json(void) {',
        '#include "passport_muse_tools.h"\n\nstatic char *build_register_json(void) {')
    s=replace_once(s,'    cJSON *commands = cJSON_CreateObject();',
        '    cJSON *commands = cJSON_CreateObject();\n    passport_muse_register(commands);')
    noise.write_text(s,encoding="utf-8")
    cmake=output/"main/CMakeLists.txt";s=cmake.read_text(encoding="utf-8")
    s=replace_once(s,'idf_component_register(','list(APPEND GADGET_REQUIRES passport_app)\n\nidf_component_register(')
    cmake.write_text(s,encoding="utf-8")
    # The scan reply is JSON: a GBK-encoded SSID is not valid UTF-8 and makes the
    # phone reject the whole list. Such a name cannot round-trip back as a JSON
    # provisioning string either, so leave it out of every scan result.
    wifi=output/"main/wifi_mgr.c";s=wifi.read_text(encoding="utf-8")
    s=replace_once(s,'#include "freertos/timers.h"\n','''#include "freertos/timers.h"
#include "passport_profile.h"

static bool passport_ssid_utf8(const char *ssid) {
    size_t length = strlen(ssid), offset = 0;
    uint32_t codepoint;
    while (offset < length)
        if (!passport_utf8_next(ssid, length, &offset, &codepoint)) return false;
    return true;
}
''')
    s=replace_once(s,"        if (ssid[0] == '\\0') continue;\n",
        "        if (ssid[0] == '\\0' || !passport_ssid_utf8(ssid)) continue;\n")
    wifi.write_text(s,encoding="utf-8")
    chat=output/"components/muse/muse_chat_link.c"
    with chat.open("a",encoding="utf-8") as f:
        f.write('\n/* Passport: called only by the owning voice worker. */\nvoid passport_muse_reply(char *out, size_t cap) { strlcpy(out, s_turn.text, cap); }\n')
    # GCC 15 (IDF 6) diagnoses format truncation more aggressively than the SDK's
    # GCC 14, and the project builds with -Werror. The debug menus really can
    # overflow their 160-byte buffer, so size it for the worst case and bound the
    # unbounded status strings instead of suppressing the diagnostic.
    menu=output/"components/muse/muse_menu.c";s=menu.read_text(encoding="utf-8")
    s=replace_once(s,'    char buf[160];\n',
        '    char buf[256];   /* worst-case status/battery text */\n')
    s=replace_once(s,'"Wi-Fi %s\\nIP    %s\\nLink  %s\\nMuse  %s\\nPhone %s\\nPower %s\\nVer   %s"',
        '"Wi-Fi %.32s\\nIP    %.15s\\nLink  %.20s\\nMuse  %.20s\\nPhone %.31s\\nPower %.15s\\nVer   %.31s"')
    menu.write_text(s,encoding="utf-8")
    shutil.copytree(ROOT/"components/bsp",output/"components/bsp",dirs_exist_ok=True)
    manifest=output/"components/bsp/idf_component.yml"
    manifest.write_text(replace_once(manifest.read_text(encoding="utf-8"),'>=5.5.3,<5.6.0','>=6.0.1,<6.1.0'),encoding="utf-8")
    # IDF 6 split the legacy driver component: driver/ledc.h now needs an explicit
    # esp_driver_ledc requirement, and freertos/task.h no longer arrives through a
    # transitive include, so both need fixing in the generated copy only.
    bsp_cmake=output/"components/bsp/CMakeLists.txt"
    bsp_cmake.write_text(replace_once(bsp_cmake.read_text(encoding="utf-8"),
        '    PRIV_REQUIRES esp_timer\n',
        '    PRIV_REQUIRES esp_timer esp_driver_ledc\n'),encoding="utf-8")
    bsp_button=output/"components/bsp/src/bsp_button.c"
    bsp_button.write_text(replace_once(bsp_button.read_text(encoding="utf-8"),
        '#include "driver/gpio.h"\n#include "freertos/task.h"\n',
        '#include "driver/gpio.h"\n#include "freertos/FreeRTOS.h"\n#include "freertos/task.h"\n'),encoding="utf-8")
    print("Prepared Muse SDK "+MUSE_REV+" at "+str(output))
    return output

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk",type=Path,required=True)
    args=parser.parse_args()
    prepare(args.sdk,ROOT/"build/muse-source")

if __name__=="__main__": main()
