import importlib.util
from pathlib import Path
import threading
import os
import struct
import unittest
import urllib.error
import urllib.request

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('voice_bridge',ROOT/'tools/passport_voice_bridge.py')
bridge=importlib.util.module_from_spec(spec);spec.loader.exec_module(bridge)

class VoiceBridgeTests(unittest.TestCase):
    def setUp(self):
        self.texts=[]
        def synth(text):self.texts.append(text);return b'\x01\x00\xff\x7f'*100
        self.server=bridge.make_server('127.0.0.1',0,'0123456789abcdef',synth)
        self.thread=threading.Thread(target=self.server.serve_forever,daemon=True);self.thread.start()
        self.url=f'http://127.0.0.1:{self.server.server_port}/tts'
    def tearDown(self):
        self.server.shutdown();self.server.server_close();self.thread.join()
    def request(self,data='你好，研究任务完成。'.encode(),token='0123456789abcdef',url=None):
        req=urllib.request.Request(url or self.url,data=data,headers={'Authorization':'Bearer '+token,'Content-Type':'text/plain; charset=utf-8'})
        return urllib.request.urlopen(req,timeout=3)
    def test_authenticated_chinese_pcm(self):
        with self.request() as response:
            self.assertEqual(response.headers['X-Passport-Audio'],'pcm_s16le_16000_mono')
            self.assertEqual(response.read(),b'\x01\x00\xff\x7f'*100)
        self.assertEqual(self.texts,['你好，研究任务完成。'])
    def test_unauthorized_not_synthesized(self):
        with self.assertRaises(urllib.error.HTTPError) as error:self.request(token='wrong')
        self.assertEqual(error.exception.code,401);self.assertEqual(self.texts,[])
    def test_input_limits_and_utf8(self):
        for data in (b'',b'x'*1024,b'\xff'):
            with self.assertRaises(urllib.error.HTTPError):self.request(data)
        self.assertEqual(self.texts,[])
    def test_only_tts_route(self):
        with self.assertRaises(urllib.error.HTTPError) as error:self.request(url=self.url+'/other')
        self.assertEqual(error.exception.code,404)
    def test_busy_is_bounded(self):
        self.server.synthesis_gate.acquire()
        try:
            with self.assertRaises(urllib.error.HTTPError) as error:self.request()
            self.assertEqual(error.exception.code,503)
        finally:self.server.synthesis_gate.release()

@unittest.skipUnless(os.name=='nt' and os.environ.get('PASSPORT_TEST_WINDOWS_SPEECH')=='1','Opt-in real Windows speech synthesis')
class WindowsSpeechTests(unittest.TestCase):
    def test_real_chinese_speech_over_http(self):
        token='0123456789abcdef'
        server=bridge.make_server('127.0.0.1',0,token)
        thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
        try:
            request=urllib.request.Request(f'http://127.0.0.1:{server.server_port}/tts',data='你好，任务已经完成。'.encode(),headers={'Authorization':'Bearer '+token})
            with urllib.request.urlopen(request,timeout=35) as response:
                self.assertEqual(response.headers['X-Passport-Audio'],bridge.FORMAT)
                pcm=response.read()
            self.assertGreater(len(pcm),32000);self.assertEqual(len(pcm)%2,0)
            self.assertGreater(max(abs(sample[0]) for sample in struct.iter_unpack('<h',pcm)),100)
        finally:server.shutdown();server.server_close();thread.join()

if __name__=='__main__':unittest.main()
