import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch
from collections import deque

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from configure_passport import PassportClient, ProtocolError, DeviceError, normalize_profile, import_profile

def reply(value):
    return b"PASSPORT/1 "+json.dumps(value).encode()+b"\n"

class Transport:
    def __init__(self,lines=()): self.lines=deque(lines); self.writes=[]
    def write(self,data): self.writes.append(data); return len(data)
    def readline(self,size=-1): return self.lines.popleft() if self.lines else b""

class ConfigureTest(unittest.TestCase):
    def test_large_font_device_validates_import_without_oversized_inventory(self):
        serial=Transport([reply({"id":1,"ok":True,"glyphs":[],"glyphs_complete":False}),
                          reply({"id":2,"ok":True})])
        import_profile(PassportClient(serial,timeout=.02),{"name":"张三"})
        self.assertEqual([json.loads(x)["op"] for x in serial.writes],["info","set"])
    def setUp(self):
        seed=patch("configure_passport.secrets.randbits",return_value=1)
        seed.start();self.addCleanup(seed.stop)
    def test_previous_session_success_cannot_confirm_a_new_time_request(self):
        with patch("secrets.randbits",side_effect=[111,222]):
            previous=PassportClient(Transport(),timeout=.01)
            serial=Transport([reply({"id":previous.next_id,"ok":True})])
            with self.assertRaises(TimeoutError):
                PassportClient(serial,timeout=.01).request("time",epoch=1761004800)
    def test_ignores_logs_and_stale_ids(self):
        serial=Transport([b"I boot log\n",reply({"id":99,"ok":True}),reply({"id":1,"ok":True})])
        self.assertTrue(PassportClient(serial,timeout=.05).request("get")["ok"])
        self.assertEqual(json.loads(serial.writes[0])["op"],"get")
    def test_malformed_response_and_bool_id_are_errors(self):
        for line in [b"PASSPORT/1 {broken}\n",reply({"id":True,"ok":True})]:
            with self.subTest(line=line), self.assertRaises(ProtocolError):
                PassportClient(Transport([line]),timeout=.02).request("get")
    def test_rejection_is_not_success(self):
        with self.assertRaisesRegex(DeviceError,"storage"):
            PassportClient(Transport([reply({"id":1,"ok":False,"error":"storage"})]),timeout=.02).request("set")
    def test_timeout(self):
        with self.assertRaises(TimeoutError): PassportClient(Transport(),timeout=.01).request("get")
    def test_validation_prevents_set_and_enforces_utf8_byte_limits(self):
        serial=Transport(); client=PassportClient(serial,timeout=.01)
        for data in [{"name":"中"*22},{"name":"bad\x00name"},{"homepage":"https://"},{"homepage":"https://host:bad/"},{"unknown":"x"}]:
            with self.subTest(data=data),self.assertRaises(ValueError): import_profile(client,data)
        self.assertEqual(serial.writes,[])
        self.assertEqual(normalize_profile({"bio":"a\nb"})["title"],"")
    def test_missing_glyphs_never_sends_set(self):
        serial=Transport([reply({"id":1,"ok":True,"glyphs":list(range(32,127))})])
        with self.assertRaisesRegex(ValueError,"font|glyph|character"):
            import_profile(PassportClient(serial,timeout=.02),{"name":"中"})
        self.assertEqual([json.loads(x)["op"] for x in serial.writes],["info"])
    def test_import_checks_then_sends_complete_replacement(self):
        serial=Transport([reply({"id":1,"ok":True,"glyphs":list(range(32,127))}),reply({"id":2,"ok":True})])
        import_profile(PassportClient(serial,timeout=.02),{"name":"Alice","homepage":"https://example.org"})
        sent=json.loads(serial.writes[1]); self.assertEqual(sent["op"],"set")
        self.assertEqual(set(sent["profile"]),{"name","title","research","bio","homepage"})
    def test_invalid_reply_ok_type(self):
        with self.assertRaises(ProtocolError):
            PassportClient(Transport([reply({"id":1,"ok":"yes"})]),timeout=.02).request("get")
    def test_rejection_never_echoes_unknown_private_values(self):
        with self.assertRaises(DeviceError) as caught:
            PassportClient(Transport([reply({"id":1,"ok":False,"error":"private-remote-value"})]),timeout=.02).request("get")
        self.assertNotIn("private-remote-value",str(caught.exception))
        with self.assertRaises(ProtocolError):
            PassportClient(Transport([reply({"id":1,"ok":False,"error":[]})]),timeout=.02).request("get")
    def test_reserved_envelope_fields_and_nonpositive_timeouts_are_rejected(self):
        with self.assertRaises(ValueError): PassportClient(Transport(),timeout=0)
        serial=Transport()
        with self.assertRaises(ValueError): PassportClient(serial,timeout=.01).request("get",id=9)
        self.assertEqual(serial.writes,[])

if __name__=="__main__": unittest.main()
