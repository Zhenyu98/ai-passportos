#!/usr/bin/env python3
"""Configure speech over the Passport USB console, without echoing its token."""
import argparse
import getpass
import json
from pathlib import Path
import time
import serial

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port',required=True)
    p.add_argument('--config',type=Path,help='Private bridge/cloud configuration JSON')
    p.add_argument('--disable',action='store_true')
    args=p.parse_args()
    if args.disable:config={'url':'','token':''}
    elif args.config:config=json.loads(args.config.read_text(encoding='utf-8'))
    else:config={'url':input('PCM speech endpoint URL: ').strip(),'token':getpass.getpass('Speech endpoint token: ')}
    if set(config)!={'url','token'} or not all(isinstance(v,str) for v in config.values()):p.error('Expected URL and token strings')
    line=('>voice_config='+json.dumps(config,ensure_ascii=True,separators=(',',':'))+'\n').encode('ascii')
    if len(line)>1024:p.error('Configuration exceeds the supported size')
    port=serial.Serial(port=None,baudrate=115200,timeout=.1)
    port.dtr=False;port.rts=False;port.port=args.port
    with port:
        port.reset_input_buffer();port.write(line);port.flush();deadline=time.monotonic()+8
        while time.monotonic()<deadline:
            reply=port.readline()
            if b'@voice_config ok' in reply:print('Speech configuration saved; credentials hidden.');return
            if b'@voice_config error' in reply:raise SystemExit('Device rejected the speech configuration')
    raise SystemExit('No acknowledgement; check the port and installed firmware')

if __name__=='__main__':main()
