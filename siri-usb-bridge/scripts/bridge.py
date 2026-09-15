#!/usr/bin/env python3
"""Siri BLE events -> USB keyboard + 48 kHz mono S16LE USB microphone.

Requires the pinned siri-remote events receiver; no LAN transport, recordings,
or shell-command mappings. Startup, USB reconnect and BLE failure release keys.
"""
import argparse
from collections import deque
import ctypes as C
import ctypes.util
import os
from pathlib import Path
import queue
import re
import signal
import subprocess
import threading
import time

ZERO = bytes(8)
SILENCE = bytes(1920)  # 20 ms, 48 kHz mono, signed 16-bit
LINE = re.compile(r'input report_id=0x([0-9a-fA-F]{2}) raw=([0-9a-fA-F ]+)(?:\||$)')

def parse_event(line):
    match = LINE.search(line)
    if not match:
        return None
    try:
        return int(match[1],16), bytes.fromhex(match[2].strip())
    except ValueError:
        return None

def key_report(mask):
    modifiers = 4 if mask & 0x20 else 0  # physical left Option
    keys = []
    if mask & 0x10:  # power -> Control + Command + Q
        modifiers |= 9
        keys.append(0x14)
    # TV: Return, Back: Escape, directions: arrows. No application-specific injection.
    for bit, key in ((1,0x28),(0x40,0x29),(0x200,0x52),(0x400,0x4f),
                     (0x800,0x51),(0x1000,0x50)):
        if mask & bit:
            keys.append(key)
    return bytes([modifiers,0]+(keys+[0]*6)[:6])

def opus_packet(payload):
    if len(payload) < 6:
        return None
    length = payload[4]
    if not 0 < length <= 94 or 5+length > len(payload):
        return None
    return int.from_bytes(payload[2:4],'little'), payload[5:5+length]

class Opus:
    def __init__(self):
        self.lib=C.CDLL(ctypes.util.find_library('opus') or 'libopus.so.0')
        self.lib.opus_decoder_create.argtypes=[C.c_int,C.c_int,C.POINTER(C.c_int)]
        self.lib.opus_decoder_create.restype=C.c_void_p
        self.lib.opus_decoder_destroy.argtypes=[C.c_void_p]
        self.lib.opus_decode.argtypes=[C.c_void_p,C.c_void_p,C.c_int,
                                      C.POINTER(C.c_int16),C.c_int,C.c_int]
        self.lib.opus_decode.restype=C.c_int
        self.decoder=None
        self.reset()

    def reset(self):
        if self.decoder:
            self.lib.opus_decoder_destroy(self.decoder)
        error=C.c_int()
        self.decoder=self.lib.opus_decoder_create(48000,1,C.byref(error))
        if error.value or not self.decoder:
            raise RuntimeError('Cannot create Opus decoder')

    def decode(self, frame):
        # Siri sends 20 ms frames; the decoder can reject malformed packets safely.
        pcm=(C.c_int16*5760)()
        data=C.create_string_buffer(frame)
        count=self.lib.opus_decode(self.decoder,data,len(frame),pcm,5760,0)
        if count < 0:
            return b''
        return C.string_at(pcm,count*2)

class Keyboard:
    def __init__(self, path):
        self.fd=os.open(path,os.O_RDWR|os.O_NONBLOCK)
        self.pending=deque([ZERO])
        self.last=ZERO

    def send(self, report):
        if report != self.last:
            if len(self.pending) >= 32:
                raise RuntimeError('USB keyboard stalled; refusing stale key replay')
            self.pending.append(report)
            self.last=report

    def clear(self):
        self.pending.clear()
        self.pending.append(ZERO)
        self.last=ZERO

    def flush(self):
        while self.pending:
            try:
                n=os.write(self.fd,self.pending[0])
                if n != 8:
                    raise RuntimeError('Short HID report')
                self.pending.popleft()
            except BlockingIOError:
                break

    def close(self):
        self.clear()
        try:
            self.flush()
        except OSError:
            pass
        os.close(self.fd)

class Audio:
    def __init__(self, device):
        self.device=device
        self.buffer=bytearray()
        self.lock=threading.Lock()
        self.enabled=False
        self.stop=threading.Event()
        self.process=None
        self.thread=threading.Thread(target=self.run,daemon=True)
        self.thread.start()

    def feed(self,pcm):
        with self.lock:
            self.buffer.extend(pcm)
            # Keep at most 100 ms. Old voice must never accumulate during USB stalls.
            if len(self.buffer)>9600:
                del self.buffer[:-9600]

    def clear(self):
        with self.lock:
            self.buffer.clear()

    def run(self):
        while not self.stop.is_set():
            if not self.enabled:
                self.stop.wait(.1)
                continue
            try:
                p=subprocess.Popen(['aplay','-q','-D',self.device,'-t','raw',
                    '-f','S16_LE','-r','48000','-c','1','--buffer-time=80000',
                    '--period-time=20000'],stdin=subprocess.PIPE)
                self.process=p
                while self.enabled and not self.stop.is_set():
                    with self.lock:
                        block=bytes(self.buffer[:1920])
                        del self.buffer[:1920]
                    p.stdin.write(block+SILENCE[len(block):])
                    p.stdin.flush()  # ALSA provides pacing; this thread may block.
            except (OSError,ValueError):
                pass
            finally:
                if self.process:
                    self.process.terminate()
                    try:
                        self.process.wait(timeout=1)
                    except subprocess.TimeoutExpired:
                        self.process.kill()
                        self.process.wait()
                    self.process=None
                self.clear()
            self.stop.wait(.5)

    def close(self):
        self.stop.set()
        self.enabled=False
        if self.process:
            self.process.terminate()
        self.thread.join(timeout=2)

def run(args):
    state=Path(args.udc_state)
    keyboard=Keyboard(args.hid)
    audio=Audio(args.alsa)
    decoder=Opus()
    messages=queue.Queue(maxsize=256)
    stop=threading.Event()
    for sig in (signal.SIGINT,signal.SIGTERM):
        signal.signal(sig,lambda *_: stop.set())
    # Explicit address avoids pairing to a neighbour's remote or changing bonds implicitly.
    proc=subprocess.Popen([args.receiver,'events','--address',args.address],
                          stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
    def reader(stream,is_error):
        for line in stream:
            if stop.is_set():
                break
            if is_error:
                # Only connection status matters; never persist raw voice reports.
                if any(s in line.lower() for s in ('disconnect','connection failed','error:')):
                    messages.put((time.monotonic(),'reset',None))
            else:
                event=parse_event(line)
                if event:
                    try:
                        messages.put_nowait((time.monotonic(),'event',event))
                    except queue.Full:
                        # Discard the burst rather than replay old audio or button presses.
                        while True:
                            try: messages.get_nowait()
                            except queue.Empty: break
                        messages.put((time.monotonic(),'reset',None))
        messages.put((time.monotonic(),'reset',None))
    for stream,err in ((proc.stdout,False),(proc.stderr,True)):
        threading.Thread(target=reader,args=(stream,err),daemon=True).start()
    connected=False
    armed=False
    mask=0
    last_event=0
    last_audio=0
    audio_held=False
    try:
        while not stop.is_set():
            now=time.monotonic()
            host=state.exists() and state.read_text().strip()=='configured'
            if host != connected:
                print('USB host connected' if host else 'USB host disconnected; releasing keys', flush=True)
                keyboard.clear(); audio.clear(); decoder.reset()
                connected=host; armed=False; mask=0; audio_held=False
                audio.enabled=host
                if not host and audio.process:
                    audio.process.terminate()
            try:
                received,kind,event=messages.get(timeout=.02)
                if now-received>.25:
                    kind,event='reset',None
            except queue.Empty:
                kind,event=None,None
            if kind=='reset':
                print('BLE input reset; releasing keys', flush=True)
                keyboard.clear(); audio.clear(); decoder.reset()
                mask=0; armed=False; audio_held=False
            elif event:
                report,payload=event
                if report==0xfb and len(payload)>=2:
                    last_event=now
                    new_mask=int.from_bytes(payload[:2],'little')
                    if connected and new_mask==0:
                        armed=True
                    mask=new_mask if armed and connected else 0
                    if not new_mask & 0x20:
                        audio_held=False
                elif report==0xfa:
                    packet=opus_packet(payload)
                    if not packet:
                        audio_held=False
                        audio.clear(); decoder.reset()
                    elif connected and armed:
                        last_event=now; last_audio=now; audio_held=True
                        pcm=decoder.decode(packet[1])
                        audio.feed(pcm)
            # Missing BLE release notifications must not leave Option or other keys stuck.
            if audio_held and now-last_audio>.2:
                audio_held=False; mask &= ~0x20; audio.clear()
            if now-last_event>2 and (mask or audio_held):
                mask=0; armed=False; audio_held=False
            if connected:
                keyboard.send(key_report(mask | (0x20 if audio_held else 0)))
                try:
                    keyboard.flush()
                except OSError:
                    keyboard.clear(); armed=False; mask=0
            if proc.poll() is not None:
                raise RuntimeError('BLE receiver exited; inspect pairing/BlueZ configuration')
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill(); proc.wait()
        audio.close()
        keyboard.close()

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--receiver',required=True)
    p.add_argument('--address',required=True)
    p.add_argument('--hid',default='/dev/hidg0')
    p.add_argument('--alsa',default='hw:CARD=UAC2Gadget,DEV=0')
    p.add_argument('--udc-state',required=True)
    run(p.parse_args())
