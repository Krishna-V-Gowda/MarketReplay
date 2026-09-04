#!/usr/bin/env python3
from __future__ import annotations
import argparse, json, random, subprocess, tempfile
from pathlib import Path
from generate_fixture import generate

def main():
    p=argparse.ArgumentParser(); p.add_argument('--binary',type=Path,required=True); p.add_argument('--mutations',type=int,default=64); p.add_argument('--output',type=Path); a=p.parse_args()
    base,_=generate(3000,7331); rng=random.Random(99173); records=[]
    with tempfile.TemporaryDirectory() as td:
        for i in range(a.mutations):
            data=bytearray(base); mode=i%4
            if mode==0: data=data[:rng.randrange(0,len(data))]
            elif mode==1:
                for _ in range(1+(i%3)): idx=rng.randrange(len(data)); data[idx]^=1<<rng.randrange(8)
            elif mode==2: idx=rng.randrange(len(data)); data[idx:idx]=bytes([rng.randrange(256)])
            else: idx=rng.randrange(len(data)); del data[idx:min(len(data),idx+1+rng.randrange(8))]
            path=Path(td)/f'mutation-{i}.itch'; path.write_bytes(data)
            result=subprocess.run([str(a.binary),str(path),'--json','--check-every','101'],capture_output=True,text=True,encoding="utf-8",errors="replace",timeout=5,check=False)
            if result.returncode not in {0,3}: raise SystemExit(f'uncontained outcome {i}: returncode={result.returncode} stderr={result.stderr[:200]}')
            records.append({'mutation':i,'mode':mode,'returncode':result.returncode})
    report={'status':'pass','mutations':len(records),'accepted':sum(r['returncode']==0 for r in records),'rejected':sum(r['returncode']==3 for r in records),'records':records}
    if a.output: a.output.parent.mkdir(parents=True,exist_ok=True); a.output.write_text(json.dumps(report,indent=2,sort_keys=True)+'\n')
    print(json.dumps({k:report[k] for k in ('status','mutations','accepted','rejected')},sort_keys=True))
if __name__=='__main__': main()
