import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("prepare_muse",ROOT/"tools/prepare_passport_muse.py")
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class PrepareMuse(unittest.TestCase):
    def test_patch_refuses_ambiguous_or_missing_anchor(self):
        self.assertEqual(module.replace_once("a START b","START","END"),"a END b")
        for source in ("none","START START"):
            with self.assertRaises(ValueError): module.replace_once(source,"START","END")

    def test_destination_must_be_ignored_build_child(self):
        with self.assertRaises(ValueError): module.validate_output(ROOT)
        with self.assertRaises(ValueError): module.validate_output(ROOT/"main")
        module.validate_output(ROOT/"build/muse-source")

    def test_unpaired_badge_without_token_keeps_ble_off(self):
        # The SDK patch must guard the one place an unpaired badge starts BLE.
        source=(ROOT/"tools/prepare_passport_muse.py").read_text(encoding="utf-8")
        guard=source.index("if (!sdk_token || !*sdk_token)")
        self.assertLess(guard,source.index("    start_ble_setup_server_if_needed();\n    enter_advertising_state(reason);\n''')"))
        self.assertIn('led_status_set_state(LED_STATE_UNPAIRED);\n        return;',source)

    def test_notes_and_replies_use_the_same_side_chat(self):
        priv='#define MUSE_HATCH_NOTE_HEAD \\\n    "{\\"message\\":\\"\\",\\"output_modality\\":\\"text\\",'
        link='muse_link_req_send(s_stream[RX_SUB], "{}", 2, true, SEND_WAIT_MS)'
        patched_priv,patched_link=module.patch_chat_session(priv,link)
        self.assertRegex(module.CHAT_SESSION,r"^[A-Za-z0-9-]{1,64}$") # the Linux client's accepted form
        self.assertIn('#define PASSPORT_CHAT_SESSION "'+module.CHAT_SESSION+'"',patched_priv)
        self.assertIn('"{\\"session_id\\":\\"" PASSPORT_CHAT_SESSION "\\",\\"message\\":',patched_priv)
        self.assertIn('"{\\"session_id\\":\\"" PASSPORT_CHAT_SESSION "\\"}"',patched_priv)
        # The length must follow the body, not the unscoped "{}" it replaces.
        self.assertIn('PASSPORT_CHAT_SUBSCRIBE, sizeof(PASSPORT_CHAT_SUBSCRIBE) - 1',patched_link)
        self.assertNotIn('"{}"',patched_link)

    def test_chat_patch_matches_the_pinned_sdk_when_present(self):
        # The default checkout location; CI without the SDK skips this.
        sdk=ROOT.parent/"scratch/muse-gadget-sdk/esp32/components/muse"
        if not (sdk/"muse_chat_priv.h").is_file(): self.skipTest("Pinned SDK checkout not present")
        module.patch_chat_session((sdk/"muse_chat_priv.h").read_text(encoding="utf-8"),
                                  (sdk/"muse_chat_link.c").read_text(encoding="utf-8"))
        module.patch_oversized_row_log((sdk/"muse_chat_link.c").read_text(encoding="utf-8"))
        noise=sdk.parents[1]/"main/noise_control.cpp"
        patched=module.patch_session_buffers(noise.read_text(encoding="utf-8"))
        self.assertIn("#define WS_RX_BUF_SIZE (SVC_FRAME_SCRATCH + 64)",patched)

    def test_session_buffers_hold_the_frame_seen_on_the_device(self):
        source=('#if SMALL_CONTROL_SESSION && CONFIG_MUSE_BOARD_AI_PASSPORT\n#define SVC_FRAME_SCRATCH (12 * 1024)\n'
                '#define WS_RX_BUF_SIZE (SVC_FRAME_SCRATCH + 1024)\n')
        patched=module.patch_session_buffers(source)
        scratch=int(patched.split("#define SVC_FRAME_SCRATCH ")[1].split("\n")[0])
        self.assertGreaterEqual(scratch+64,16424)    # the WebSocket payload as measured
        self.assertGreaterEqual(scratch,16424-16)    # decrypted: less the AEAD tag

if __name__=="__main__": unittest.main()
