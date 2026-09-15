"""Upgrade this explicitly identified pad, preserving NVS; no port auto-selection."""
import argparse, pathlib, subprocess, sys, time, serial
from serial.tools import list_ports
parser=argparse.ArgumentParser()
parser.add_argument("--port", default="/dev/cu.usbmodem21304")
parser.add_argument("--download-port", default="/dev/cu.usbmodem21301")
args=parser.parse_args()
info=next((p for p in list_ports.comports() if p.device==args.port),None)
if not info or info.vid!=0xcafe or info.pid!=0x4015 or info.serial_number!="1020BA4658B4-PAD1":
 raise SystemExit("Expected Siri Voice Pad not found on the specified port")
root=pathlib.Path(__file__).resolve().parents[2]
app=root/'esp32-siri-pad/.pio/build/pad/firmware.bin'
assert app.is_file()
# Firmware CDC uses DTR, but its control lines are not hardware reset lines.
s=serial.Serial(port=None,baudrate=115200,timeout=.2);s.dtr=False;s.rts=False;s.port=args.port;s.open();s.dtr=True;s.write(b'B');s.flush();s.close()
port=args.download_port
for _ in range(100):
 if pathlib.Path(port).exists():break
 time.sleep(.1)
base=[sys.executable,'-m','esptool','--chip','esp32s3','--port',port,'--before','no-reset','--connect-attempts','2']
check=subprocess.run(base+['--after','no-reset','read-mac'],capture_output=True,text=True,timeout=20)
if check.returncode or '10:20:ba:46:58:b4' not in check.stdout:raise SystemExit('Target identity verification failed: '+check.stdout+check.stderr)
print('Verified target 10:20:ba:46:58:b4',flush=True)
subprocess.run(base+['--after','no-reset','--baud','460800','write-flash','0x10000',str(app)],check=True,timeout=60)
subprocess.run(base+['--after','watchdog-reset','read-mac'],check=True,timeout=20)
# Complete USB/UART reset with inactive control lines, as validated by the
# earlier home-task-panel project on the exact same hardware.
log=[]
for _ in range(30):
 try:
  boot=serial.Serial(port=None,baudrate=115200,timeout=.1);boot.dtr=False;boot.rts=False;boot.port=port;boot.open();break
 except serial.SerialException:time.sleep(.05)
else:boot=None
if boot:
 end=time.monotonic()+4
 try:
  while time.monotonic()<end:
   b=boot.readline()
   if b:log.append(b.decode(errors='replace'))
 except serial.SerialException:pass
 finally:boot.close()
(root/'esp32-siri-pad/logs').mkdir(exist_ok=True)
(root/'esp32-siri-pad/logs/upgrade-boot.log').write_text(''.join(log))
print('Upgrade and boot transition finished',flush=True)
