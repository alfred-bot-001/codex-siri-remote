"""Generate the published SiriPadCJK subset from the documented OFL font.

Requires fonttools and lv_font_conv 1.5.3 (installed with npm).
Usage: python tools/generate_font.py /path/to/NotoSansSC.ttf
"""
import argparse
import hashlib
import pathlib
import shutil
import subprocess
import tempfile
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont

p = argparse.ArgumentParser()
p.add_argument('font', type=pathlib.Path)
a = p.parse_args()
expected = 'a3041811a78c361b1de50f953c805e0244951c21c5bd412f7232ef0d899af0da'
if hashlib.sha256(a.font.read_bytes()).hexdigest() != expected:
    raise SystemExit('Font differs from the source documented in THIRD_PARTY.md')
converter = shutil.which('lv_font_conv')
if not converter:
    raise SystemExit('Install lv_font_conv 1.5.3 and add it to PATH')
output = pathlib.Path(__file__).resolve().parents[1] / 'main/font_cn20.c'
with tempfile.TemporaryDirectory() as tmp:
    font = TTFont(a.font)
    static = pathlib.Path(tmp) / 'SiriPadCJK.ttf'
    instantiateVariableFont(font, {'wght': 500}, inplace=True).save(static)
    subprocess.run([converter, '--font', str(static), '--size', '20', '--bpp', '4',
                    '--format', 'lvgl', '--symbols',
                    '语音输入已连接未连接正在连接左移右移回车板载遥控器麦克风USB等待按住说话点击配对停止法切换准备就绪聆听电脑',
                    '--no-compress', '--lv-include', 'lvgl.h', '-o', str(output)], check=True)
# Do not embed machine-specific converter paths in the published generated file.
text = output.read_text()
lines = text.splitlines()
lines = [' * Source: SiriPadCJK; see THIRD_PARTY.md and tools/generate_font.py' if line.startswith(' * Opts:') else line for line in lines]
output.write_text('\n'.join(lines) + '\n')
