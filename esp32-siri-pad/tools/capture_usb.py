"""Bounded recording of this USB microphone, never the default microphone."""
import argparse, array, json, math, pathlib, subprocess, sys, wave
p=argparse.ArgumentParser();p.add_argument('--seconds',type=int,default=5);p.add_argument('--output',required=True);a=p.parse_args()
if not 1<=a.seconds<=120:raise SystemExit('seconds must be 1..120')
out=pathlib.Path(a.output);out.parent.mkdir(parents=True,exist_ok=True)
command=['/opt/homebrew/bin/ffmpeg','-hide_banner','-nostdin','-y','-f','avfoundation','-i',':Siri Voice Pad Microphone','-t',str(a.seconds),'-ar','48000','-ac','1','-c:a','pcm_s16le',str(out)]
try:
 result=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=a.seconds+15,text=True)
except subprocess.TimeoutExpired as e:
 raise SystemExit('USB microphone did not deliver audio within the time limit') from e
out.with_suffix('.ffmpeg.log').write_text(result.stderr)
if result.returncode:print(result.stderr);raise SystemExit(result.returncode)
with wave.open(str(out)) as f:
 samples=array.array('h',f.readframes(f.getnframes()));rate=f.getframerate();channels=f.getnchannels()
if sys.byteorder!='little':samples.byteswap()
peak=max(map(abs,samples),default=0);rms=math.sqrt(sum(int(x)*x for x in samples)/max(1,len(samples)))
report={'samples':len(samples),'rate':rate,'channels':channels,'duration':len(samples)/rate/channels,'peak':peak,'rms':round(rms,2),'nonzero':sum(x!=0 for x in samples)}
out.with_suffix('.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))
