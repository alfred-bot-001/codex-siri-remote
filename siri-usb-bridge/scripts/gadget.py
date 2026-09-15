#!/usr/bin/env python3
"""Configure one local development USB keyboard + capture-only UAC2 device.

Does not change boot settings, connect Bluetooth, or send any keystrokes.
Run only after verifying USB-C data wiring and an adequate power supply.
"""
import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path('/sys/kernel/config/usb_gadget')
GADGET = ROOT / 'siri_usb'
KEYBOARD = bytes.fromhex(
    '05 01 09 06 a1 01 05 07 19 e0 29 e7 15 00 25 01 '
    '75 01 95 08 81 02 95 01 75 08 81 01 '
    '95 05 75 01 05 08 19 01 29 05 91 02 '
    '95 01 75 03 91 01 95 06 75 08 15 00 25 65 '
    '05 07 19 00 29 65 81 00 c0')

def write(path, value):
    path.write_bytes(value if isinstance(value, bytes) else str(value).encode())

def setup(bind=True):
    if os.geteuid() != 0:
        raise SystemExit('Run with sudo.')
    controllers = list(Path('/sys/class/udc').iterdir())
    if bind and len(controllers) != 1:
        raise SystemExit('Expected one USB device controller; configure dwc2 peripheral mode first.')
    subprocess.run(['/sbin/modprobe', 'libcomposite'], check=True)
    if not ROOT.exists():
        raise SystemExit('configfs is not mounted at /sys/kernel/config.')
    if list(ROOT.iterdir()):
        raise SystemExit('A gadget already exists. Refusing to replace an existing configuration.')
    GADGET.mkdir()
    # Linux Foundation composite gadget IDs: local prototype only, not a retail VID/PID.
    for key, value in {'idVendor':'0x1d6b', 'idProduct':'0x0104',
                       'bcdDevice':'0x0100', 'bcdUSB':'0x0200',
                       'bDeviceClass':'0xef','bDeviceSubClass':'0x02',
                       'bDeviceProtocol':'0x01'}.items():
        write(GADGET/key, value)
    strings = GADGET/'strings/0x409'
    strings.mkdir()
    for key,value in {'serialnumber':'agent004-siri-usb-001',
                      'manufacturer':'Local prototype',
                      'product':'Siri USB Keyboard and Microphone'}.items():
        write(strings/key,value)
    config = GADGET/'configs/c.1'
    config.mkdir()
    (config/'strings/0x409').mkdir()
    write(config/'strings/0x409/configuration','Keyboard + Microphone')
    # USB 2.0 bus-power budget; this declaration cannot negotiate 5 A for Pi 5.
    write(config/'MaxPower','500')
    keyboard = GADGET/'functions/hid.usb0'
    keyboard.mkdir()
    for key,value in {'protocol':1,'subclass':1,'report_length':8}.items():
        write(keyboard/key,value)
    write(keyboard/'report_desc', KEYBOARD)
    audio = GADGET/'functions/uac2.usb0'
    audio.mkdir()
    # p_* is gadget-side playback -> Mac capture; c_* would be Mac speakers.
    for key,value in {'p_chmask':1,'p_srate':48000,'p_ssize':2,'c_chmask':0}.items():
        write(audio/key,value)
    for key in ('p_mute_present','p_volume_present','c_mute_present','c_volume_present'):
        if (audio/key).exists():
            write(audio/key,0)
    (config/'hid.usb0').symlink_to(keyboard)
    (config/'uac2.usb0').symlink_to(audio)
    if bind:
        write(GADGET/'UDC',controllers[0].name)
    print('Gadget configured, bound=' + str(bind) + '; no key events or recording started.')

def stop():
    if not GADGET.exists():
        return
    if (GADGET/'UDC').read_text().strip():
        write(GADGET/'UDC','\n')
    for name in ('hid.usb0','uac2.usb0'):
        (GADGET/'configs/c.1'/name).unlink(missing_ok=True)
    (GADGET/'configs/c.1/strings/0x409').rmdir()
    (GADGET/'configs/c.1').rmdir()
    for name in ('hid.usb0','uac2.usb0'):
        (GADGET/'functions'/name).rmdir()
    (GADGET/'strings/0x409').rmdir()
    GADGET.rmdir()

if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('action', choices=['prepare','start','stop'])
    args=p.parse_args()
    if args.action=='stop':
        stop()
    else:
        setup(bind=args.action=='start')
