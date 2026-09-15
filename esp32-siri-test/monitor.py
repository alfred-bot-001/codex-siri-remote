"""Bounded serial diagnostics; commands can be appended to a local control file."""
import argparse
import pathlib
import time
import serial

p = argparse.ArgumentParser()
p.add_argument("--port", required=True)
p.add_argument("--seconds", type=int, default=600)
p.add_argument("--log", required=True)
p.add_argument("--control", required=True)
a = p.parse_args()
control = pathlib.Path(a.control)
control.write_text("")
offset = 0
deadline = time.monotonic() + a.seconds
with serial.Serial(a.port, 115200, timeout=0.3) as connection, open(a.log, "a", buffering=1) as log:
    connection.dtr = True
    connection.rts = False
    while time.monotonic() < deadline:
        data = connection.readline()
        if data:
            line = time.strftime("%Y-%m-%d %H:%M:%S ") + data.decode("utf-8", errors="replace").rstrip()
            log.write(line + "\n")
            print(line, flush=True)
        commands = control.read_text()
        if len(commands) > offset:
            new = commands[offset:]
            offset = len(commands)
            connection.write("".join(c for c in new if c in "sd").encode())
