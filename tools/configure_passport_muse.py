#!/usr/bin/env python3
"""Set a Muse SDK token over the existing USB console without printing it."""
import argparse
import getpass
from pathlib import Path
import time
import serial

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port',required=True)
    p.add_argument('--token-file',type=Path)
    args=p.parse_args()
    token=args.token_file.read_text(encoding='utf-8').strip() if args.token_file else getpass.getpass('Muse SDK token: ')
    if not token.startswith('mgst_') or not token.isascii() or not 8<len(token)<2048 or any(c.isspace() for c in token):
        raise SystemExit('Invalid SDK token format')
    # Do not change DTR/RTS: this command intentionally restarts only after saving.
    port=serial.Serial(port=None,baudrate=115200,timeout=.1)
    port.dtr=False;port.rts=False;port.port=args.port
    with port:
        port.reset_input_buffer();port.write(('>sdk_token='+token+'\n').encode('ascii'));port.flush()
        deadline=time.monotonic()+8
        while time.monotonic()<deadline:
            line=port.readline()
            if b'@sdk_token ok' in line:
                print('SDK token saved; device restarting. Pair it in the Muse app.');return
            if b'@sdk_token error' in line: raise SystemExit('Device rejected SDK token')
    raise SystemExit('No token acknowledgement; check the port and installed Muse firmware')

if __name__=='__main__': main()
