#!/usr/bin/env python3
"""Configure one explicitly selected Passport over USB; no port auto-probing."""
from __future__ import annotations
import argparse
import json
import math
import secrets
from pathlib import Path
import time
import unicodedata
from urllib.parse import urlsplit

LIMITS = {"name":64, "title":96, "research":192, "bio":512, "homepage":128}
PREFIX = b"PASSPORT/1 "
LINE_MAX = 4096
ERROR_CODES = {"invalid","not_found","corrupt","storage","capacity","unsupported"}

class ProtocolError(RuntimeError):
    pass
class DeviceError(RuntimeError):
    pass

def normalize_profile(data: object) -> dict[str,str]:
    if not isinstance(data,dict) or set(data)-LIMITS.keys():
        raise ValueError("Profile must contain only the five supported fields")
    result={}
    for name, maximum in LIMITS.items():
        value=data.get(name,"")
        if not isinstance(value,str):
            raise ValueError("Expected text in field: "+name)
        try:
            encoded=value.encode("utf-8")
        except UnicodeEncodeError:
            raise ValueError("Invalid Unicode in field: "+name) from None
        if len(encoded)>maximum:
            raise ValueError("UTF-8 byte limit exceeded in field: "+name)
        for char in value:
            if unicodedata.category(char)=="Cc" and not (name in {"bio","research"} and char=="\n"):
                raise ValueError("Unsupported control character in field: "+name)
        result[name]=value
    url=result["homepage"]
    if url:
        try:
            parsed=urlsplit(url)
            if parsed.scheme not in {"http","https"} or not parsed.hostname:
                raise ValueError
            if any(ord(c)<=32 or c=="\\" for c in url) or parsed.netloc.endswith(":"):
                raise ValueError
            if parsed.port==0:
                raise ValueError
            for i,char in enumerate(url):
                if char=="%" and (len(url)-i<3 or any(c not in "0123456789abcdefABCDEF" for c in url[i+1:i+3])):
                    raise ValueError
            if ":" not in parsed.hostname and any(not (c.isalnum() or ord(c)>=128 or c in ".-_~%") for c in parsed.hostname):
                raise ValueError
        except ValueError:
            raise ValueError("Invalid HTTP/HTTPS homepage") from None
    return result

class PassportClient:
    def __init__(self,transport,timeout: float=3.0):
        if not isinstance(timeout,(int,float)) or not math.isfinite(timeout) or timeout<=0:
            raise ValueError("Timeout must be a positive finite number")
        self.transport=transport
        self.timeout=timeout
        self.next_id=secrets.randbits(32)

    def request(self,operation: str,**payload):
        if {"id","op"} & payload.keys():
            raise ValueError("Request envelope fields are reserved")
        identifier=self.next_id
        self.next_id=(identifier+1)&0xffffffff
        body={"id":identifier,"op":operation,**payload}
        raw=json.dumps(body,ensure_ascii=False,separators=(",",":")).encode("utf-8")+b"\n"
        if len(raw)-1>LINE_MAX:
            raise ValueError("Request exceeds the device line limit")
        if self.transport.write(raw)!=len(raw):
            raise OSError("Incomplete USB write")
        deadline=time.monotonic()+self.timeout
        while time.monotonic()<deadline:
            line=self.transport.readline(LINE_MAX+len(PREFIX)+2)
            if not line.startswith(PREFIX):
                continue
            if len(line)>LINE_MAX or not line.endswith(b"\n"):
                raise ProtocolError("Truncated or oversize device response")
            try:
                value=json.loads(line[len(PREFIX):].decode("utf-8"))
            except (ValueError,UnicodeError):
                raise ProtocolError("Malformed device response") from None
            if not isinstance(value,dict) or type(value.get("id")) is not int:
                raise ProtocolError("Invalid device response identifier")
            if value["id"]!=identifier:
                continue
            if type(value.get("ok")) is not bool:
                raise ProtocolError("Invalid device response status")
            if not value["ok"]:
                code=value.get("error")
                if not isinstance(code,str):
                    raise ProtocolError("Invalid device rejection code")
                raise DeviceError("Device rejected request: "+(code if code in ERROR_CODES else "rejected"))
            return value
        raise TimeoutError("No matching device response; saved state is unconfirmed")

def import_profile(client: PassportClient,data: object):
    profile=normalize_profile(data)
    info=client.request("info")
    glyphs=info.get("glyphs")
    if not isinstance(glyphs,list) or len(glyphs)>1024 or any(type(x) is not int or not 0<=x<=0x10ffff for x in glyphs):
        raise ProtocolError("Invalid device glyph inventory")
    supported=set(glyphs)
    required={ord(c) for value in profile.values() for c in value if c!="\n"}
    complete=info.get("glyphs_complete",True)
    if type(complete) is not bool:
        raise ProtocolError("Invalid device glyph coverage policy")
    if complete and not required<=supported:
        raise ValueError("Unsupported profile characters; rebuild the font subset before import")
    return client.request("set",profile=profile)

def unique_pairs(pairs):
    result={}
    for key,value in pairs:
        if key in result:
            raise ValueError("Duplicate JSON key")
        result[key]=value
    return result

def connect_serial(port: str):
    import serial
    connection=serial.Serial(port=None,baudrate=115200,timeout=.15,write_timeout=2)
    connection.dtr=False
    connection.rts=False
    connection.port=port
    connection.open()
    return connection

def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port",required=True,help="Explicitly selected device port, e.g. COM7")
    parser.add_argument("--timeout",type=float,default=3.0)
    commands=parser.add_subparsers(dest="command",required=True)
    commands.add_parser("info")
    commands.add_parser("get")
    set_command=commands.add_parser("set")
    set_command.add_argument("--profile",type=Path,required=True)
    time_command=commands.add_parser("time")
    time_command.add_argument("--now",action="store_true",required=True)
    args=parser.parse_args()
    try:
        profile=None
        if args.command=="set":
            with args.profile.open(encoding="utf-8-sig") as stream:
                profile=normalize_profile(json.load(stream,object_pairs_hook=unique_pairs))
        with connect_serial(args.port) as serial:
            client=PassportClient(serial,args.timeout)
            if args.command=="set":
                result=import_profile(client,profile)
            elif args.command=="time":
                result=client.request("time",epoch=int(time.time()))
            else:
                result=client.request(args.command)
        print(json.dumps(result,ensure_ascii=False,indent=2))
        return 0
    except (OSError,ValueError,RuntimeError,TimeoutError) as error:
        parser.exit(1,str(error)+"\n")

if __name__=="__main__":
    raise SystemExit(main())
