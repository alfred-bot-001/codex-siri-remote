"""Capture one bounded Siri Remote Opus test over ESP32 USB serial, locally."""
import argparse
import array
import ctypes as C
import ctypes.util
import json
import math
from pathlib import Path
import time
import wave
import serial


class Decoder:
    def __init__(self):
        self.lib = C.CDLL(ctypes.util.find_library("opus") or "/opt/homebrew/lib/libopus.dylib")
        self.lib.opus_decoder_create.argtypes = [C.c_int, C.c_int, C.POINTER(C.c_int)]
        self.lib.opus_decoder_create.restype = C.c_void_p
        self.lib.opus_decode.argtypes = [C.c_void_p, C.c_void_p, C.c_int,
                                       C.POINTER(C.c_int16), C.c_int, C.c_int]
        self.lib.opus_decode.restype = C.c_int
        self.lib.opus_decoder_destroy.argtypes = [C.c_void_p]
        error = C.c_int()
        self.state = self.lib.opus_decoder_create(48000, 1, C.byref(error))
        if error.value or not self.state:
            raise RuntimeError(f"Opus initialization failed: {error.value}")

    def decode(self, frame):
        output = (C.c_int16 * 5760)()
        data = C.create_string_buffer(frame)
        n = self.lib.opus_decode(self.state, data, len(frame), output, 5760, 0)
        if n < 0:
            raise ValueError(f"Opus decode error {n}")
        return C.string_at(output, n * 2)

    def close(self):
        if self.state:
            self.lib.opus_decoder_destroy(self.state)
            self.state = None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--control", required=True)
    args = parser.parse_args()
    base = Path(args.output)
    base.parent.mkdir(parents=True, exist_ok=True)
    control = Path(args.control)
    control.write_text("")
    decoder = Decoder()
    # Verify decoder operation with a standard Opus silence packet before recording.
    if not decoder.decode(bytes.fromhex("f8fffe")):
        raise RuntimeError("Decoder self-check failed")
    decoder.close()
    decoder = Decoder()
    pcm = bytearray()
    stats = dict(packets=0, decoded_frames=0, malformed=0, decode_errors=0,
                 missing_packets=0, duplicate_or_reordered=0, sentinels=0)
    expected = None
    last_audio = None
    armed_at = None
    offset = 0
    until = time.monotonic() + 240
    try:
        with serial.Serial(args.port, 115200, timeout=0.2) as port, \
                open(base.with_suffix(".log"), "w", buffering=1) as log:
            port.dtr = True
            port.rts = False
            def report(line):
                text = time.strftime("%Y-%m-%d %H:%M:%S ") + line
                log.write(text + "\n")
                print(text, flush=True)
            report("CAPTURE_MONITOR_READY; waiting for explicit m command")
            try:
                while time.monotonic() < until:
                    line = port.readline().decode("ascii", errors="replace").strip()
                    if line.startswith("AUDIO "):
                        if armed_at is None:
                            continue
                        stats["packets"] += 1
                        try:
                            payload = bytes.fromhex(line[6:])
                            if len(payload) < 6:
                                raise ValueError("short report")
                            length = payload[4]
                            if length == 0:
                                stats["sentinels"] += 1
                                continue
                            if length > 94 or 5 + length > len(payload):
                                raise ValueError("invalid length")
                        except ValueError:
                            stats["malformed"] += 1
                            continue
                        sequence = int.from_bytes(payload[2:4], "little")
                        if expected is not None:
                            gap = (sequence - expected) & 0xffff
                            if gap > 32767:
                                stats["duplicate_or_reordered"] += 1
                                continue
                            stats["missing_packets"] += gap
                        expected = (sequence + 1) & 0xffff
                        try:
                            decoded = decoder.decode(payload[5:5 + length])
                        except ValueError:
                            stats["decode_errors"] += 1
                            continue
                        pcm.extend(decoded)
                        stats["decoded_frames"] += 1
                        last_audio = time.monotonic()
                        if stats["decoded_frames"] % 50 == 0:
                            report(f"AUDIO_PROGRESS decoded_frames={stats['decoded_frames']}")
                        if len(pcm) >= 48000 * 2 * 30:
                            report("30-second recording limit reached")
                            break
                    elif line:
                        report(line)
                    commands = control.read_text()
                    if len(commands) > offset:
                        new = commands[offset:]
                        offset = len(commands)
                        for command in new:
                            if command == "m" and armed_at is None:
                                armed_at = time.monotonic()
                                port.write(b"m")
                                report("ARM_REQUESTED")
                            elif command in "sdx":
                                port.write(command.encode())
                    if last_audio and time.monotonic() - last_audio > 1.5:
                        report("Audio stream ended; finishing local recording")
                        break
                    if armed_at and time.monotonic() - armed_at > 65:
                        report("Recording window expired")
                        break
            finally:
                port.write(b"x")
                stop_until = time.monotonic() + 1
                while time.monotonic() < stop_until:
                    line = port.readline().decode("ascii", errors="replace").strip()
                    if line and not line.startswith("AUDIO "):
                        report(line)
    finally:
        decoder.close()
    stats["seconds"] = len(pcm) / 96000
    if pcm:
        samples = array.array("h", pcm)
        peak = max(abs(x) for x in samples)
        rms = math.sqrt(sum(x * x for x in samples) / len(samples))
        stats.update(peak=peak, rms=rms,
                     peak_dbfs=20 * math.log10(max(peak, 1) / 32768),
                     rms_dbfs=20 * math.log10(max(rms, 1) / 32768),
                     clipped_samples=sum(abs(x) >= 32767 for x in samples))
        with wave.open(str(base.with_suffix(".wav")), "wb") as wav:
            wav.setnchannels(1)
            wav.setsampwidth(2)
            wav.setframerate(48000)
            wav.writeframes(pcm)
    base.with_suffix(".json").write_text(json.dumps(stats, indent=2) + "\n")
    print(json.dumps(stats), flush=True)
    return 0 if stats["decoded_frames"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
