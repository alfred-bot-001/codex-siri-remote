"""Read bounded device diagnostics, or an idle-only LVGL framebuffer snapshot."""
import argparse, pathlib, time, serial
p=argparse.ArgumentParser();p.add_argument('--port',default='/dev/cu.usbmodem21304');p.add_argument('--seconds',type=int,default=15);p.add_argument('--snapshot');p.add_argument('--command',choices=['S','X','B','D','L','V']);p.add_argument('--log');a=p.parse_args()
connection=serial.Serial(port=None,baudrate=115200,timeout=.3)
connection.dtr=False;connection.rts=False;connection.port=a.port
with connection as s:
 s.dtr=True
 if a.command:s.write(a.command.encode())
 if a.snapshot:s.write(b'P')
 end=time.monotonic()+a.seconds
 log=open(a.log,'a',buffering=1) if a.log else None
 while time.monotonic()<end:
  try:line=s.readline()
  except serial.SerialException:
   if a.command=='B':print('Device entered upgrade transition');break
   raise
  if line.startswith(b'FRAME '):
   _,w,h,n=line.decode().split();data=bytearray();deadline=time.monotonic()+20
   while len(data)<int(n) and time.monotonic()<deadline:data.extend(s.read(int(n)-len(data)))
   if len(data)!=int(n):raise RuntimeError(f'Short snapshot {len(data)}/{n}')
   pathlib.Path(a.snapshot).write_bytes(data);print(f'Snapshot {w}x{h}, {len(data)} bytes');break
  if line:
   text=line.decode(errors='replace').rstrip();print(text,flush=True)
   if log:log.write(time.strftime('%Y-%m-%d %H:%M:%S ')+text+'\n')
