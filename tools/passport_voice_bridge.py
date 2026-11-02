#!/usr/bin/env python3
"""Optional authenticated Windows speech bridge. Requires a running Windows PC."""
from __future__ import annotations
import argparse
import hmac
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import secrets
import subprocess
import tempfile
import threading
import wave

ROOT=Path(__file__).resolve().parents[1]
FORMAT='pcm_s16le_16000_mono'
MAX_TEXT=1023
MAX_PCM=16000*2*120

def synthesize(text: str,voice: str | None=None) -> bytes:
    if os.name!='nt':raise RuntimeError('The local bridge requires Windows')
    with tempfile.TemporaryDirectory(prefix='passport-speech-') as folder:
        source=Path(folder)/'input.txt';target=Path(folder)/'speech.wav'
        source.write_text(text,encoding='utf-8')
        command=['powershell.exe','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',str(ROOT/'tools/passport_speech_sapi.ps1'),
                 '-InputFile',str(source),'-OutputFile',str(target)]
        if voice:command+=['-Voice',voice]
        result=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=30,
                              creationflags=subprocess.CREATE_NO_WINDOW)
        if result.returncode:raise RuntimeError('Windows speech synthesis failed; check installed Chinese voices')
        with wave.open(str(target),'rb') as audio:
            if audio.getnchannels()!=1 or audio.getsampwidth()!=2 or audio.getframerate()!=16000:
                raise RuntimeError('Unexpected speech format')
            if audio.getnframes()>MAX_PCM//2:raise RuntimeError('Speech exceeds two minutes')
            pcm=audio.readframes(audio.getnframes())
    if not pcm or len(pcm)%2:raise RuntimeError('Empty or truncated speech')
    return pcm

def make_server(host: str,port: int,token: str,synth=synthesize) -> ThreadingHTTPServer:
    if len(token)<16 or any(ord(c)<33 or ord(c)>126 for c in token):raise ValueError('A private token of at least 16 ASCII characters is required')
    gate=threading.BoundedSemaphore(1)
    class Handler(BaseHTTPRequestHandler):
        protocol_version='HTTP/1.0'
        def log_message(self,*args):pass  # No request text, credentials or query logs.
        def do_POST(self):
            self.connection.settimeout(3)
            if self.path!='/tts':self.send_error(404);return
            supplied=self.headers.get('Authorization','').encode('utf-8')
            if not hmac.compare_digest(supplied,('Bearer '+token).encode('ascii')):
                self.send_error(401);return
            try:
                if self.headers.get('Transfer-Encoding'):raise ValueError('Chunked input is unsupported')
                lengths=self.headers.get_all('Content-Length',[])
                if len(lengths)!=1:raise ValueError('Exactly one input length is required')
                size=int(lengths[0])
                if not 0<size<=MAX_TEXT:raise ValueError('Text is empty or too long')
                raw=self.rfile.read(size)
                if len(raw)!=size:raise ValueError('Truncated input')
                text=raw.decode('utf-8')
                if not text.strip() or '\x00' in text:raise ValueError('Empty or invalid input')
            except (ValueError,UnicodeError,TimeoutError):self.send_error(400);return
            if not gate.acquire(blocking=False):self.send_error(503);return
            try:
                pcm=synth(text)
                if not isinstance(pcm,bytes) or not 0<len(pcm)<=MAX_PCM or len(pcm)%2:raise ValueError('Invalid speech')
                self.send_response(200);self.send_header('Content-Type','audio/pcm')
                self.send_header('X-Passport-Audio',FORMAT);self.send_header('Content-Length',str(len(pcm)))
                self.send_header('Cache-Control','no-store');self.end_headers()
                for offset in range(0,len(pcm),4096):self.wfile.write(pcm[offset:offset+4096])
            except (BrokenPipeError,ConnectionResetError,TimeoutError):pass
            except Exception:
                self.send_error(502,'Speech synthesis failed')
            finally:gate.release()
    class Server(ThreadingHTTPServer):
        def get_request(self):
            connection,address=super().get_request();connection.settimeout(3);return connection,address
    server=Server((host,port),Handler)
    server.daemon_threads=True;server.synthesis_gate=gate
    return server

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--host',default='127.0.0.1',help='Use 0.0.0.0 explicitly to accept device connections')
    p.add_argument('--port',type=int,default=8765)
    p.add_argument('--device-host',help='This PC private IPv4 address reachable by Passport')
    p.add_argument('--config',type=Path,default=ROOT/'build/passport-voice.local.json')
    p.add_argument('--voice')
    p.add_argument('--sample',type=Path,help='Write a Chinese WAV sample and exit; no server or credentials')
    args=p.parse_args()
    if args.sample:
        pcm=synthesize('你好，我是你的 Passport 助手。语音回复已经准备好了。',args.voice)
        args.sample.parent.mkdir(parents=True,exist_ok=True)
        with wave.open(str(args.sample),'wb') as out:
            out.setnchannels(1);out.setsampwidth(2);out.setframerate(16000);out.writeframes(pcm)
        print(f'Chinese speech sample: {len(pcm)/32000:.2f}s, PCM16 mono 16000 Hz');return
    if not args.device_host:p.error('--device-host is required to generate the device configuration')
    if args.config.exists():
        config=json.loads(args.config.read_text(encoding='utf-8'));token=config['token']
    else:token=secrets.token_hex(24)
    config={'url':f'http://{args.device_host}:{args.port}/tts','token':token}
    args.config.parent.mkdir(parents=True,exist_ok=True)
    args.config.write_text(json.dumps(config,ensure_ascii=False)+'\n',encoding='utf-8')
    server=make_server(args.host,args.port,token,lambda text:synthesize(text,args.voice))
    print(f'Windows speech bridge: {args.host}:{args.port}. Keep this PC running.')
    print(f'Private USB configuration saved to {args.config}; token hidden.')
    try:server.serve_forever()
    except KeyboardInterrupt:pass
    finally:server.server_close()

if __name__=='__main__':main()
