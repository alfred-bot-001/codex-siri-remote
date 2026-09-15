"""Convert an explicit CoreAudio float recording to WAV, preserving internal pauses."""
import argparse
import array
import json
import math
import pathlib
import sys
import wave

p = argparse.ArgumentParser()
p.add_argument('input', type=pathlib.Path)
p.add_argument('output', type=pathlib.Path)
a = p.parse_args()
data = array.array('f')
data.frombytes(a.input.read_bytes())
if sys.byteorder != 'little':
    data.byteswap()
if any(not math.isfinite(x) for x in data):
    raise SystemExit('Invalid audio samples')
active = [i for i, x in enumerate(data) if abs(x) > 1 / 32768]
start = max(0, active[0] - 24000) if active else 0
end = min(len(data), active[-1] + 24001) if active else len(data)
# Trim only waiting time outside the recording; keep all speech pauses intact.
segment = data[start:end]
pcm = array.array('h', (max(-32768, min(32767, round(x * 32768))) for x in segment))
if sys.byteorder != 'little':
    pcm.byteswap()
with wave.open(str(a.output), 'wb') as wav:
    wav.setnchannels(1)
    wav.setsampwidth(2)
    wav.setframerate(48000)
    wav.writeframes(pcm.tobytes())
report = {'input_samples': len(data), 'input_seconds': len(data) / 48000,
          'trim_start_seconds': start / 48000, 'playback_seconds': len(segment) / 48000,
          'peak': max(map(abs, segment), default=0),
          'rms': math.sqrt(sum(x*x for x in segment) / max(1, len(segment))),
          'gain_applied': False}
a.output.with_suffix('.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report))
