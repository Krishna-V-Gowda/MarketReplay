#!/usr/bin/env python3
from __future__ import annotations
import argparse, importlib.util, json, subprocess, sys, tempfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from generate_fixture import generate
spec=importlib.util.spec_from_file_location('oracle',ROOT/'reference'/'oracle.py'); oracle=importlib.util.module_from_spec(spec); assert spec.loader; sys.modules['oracle']=oracle; spec.loader.exec_module(oracle)

def main():
    p=argparse.ArgumentParser(); p.add_argument('--binary',type=Path,required=True); p.add_argument('--scenarios',type=int,default=12); p.add_argument('--output',type=Path); a=p.parse_args()
    records=[]
    with tempfile.TemporaryDirectory() as td:
        for i in range(a.scenarios):
            events=1+[3,17,113,997,5000][i%5]; seed=301+i*17; data,manifest=generate(events,seed); path=Path(td)/f'{i}.itch'; path.write_bytes(data)
            cpp=subprocess.run([str(a.binary),str(path),'--json','--strict-time','--check-every','257'],capture_output=True,text=True,check=False)
            if cpp.returncode!=0: raise SystemExit(f'C++ failed scenario {i}: {cpp.stderr}')
            expected=oracle.replay_stream(path.open('rb'),strict_time=True,check_every=257); actual=json.loads(cpp.stdout)
            if actual!=expected: raise SystemExit(f'differential mismatch scenario {i}\nactual={actual}\nexpected={expected}')
            records.append({'scenario':i,'events':events,'seed':seed,'sha256':manifest['sha256'],'fingerprint':actual['fingerprint']})
    report={'status':'pass','scenarios':len(records),'records':records}
    text=json.dumps(report,indent=2,sort_keys=True)+'\n'
    if a.output: a.output.parent.mkdir(parents=True,exist_ok=True); a.output.write_text(text)
    print(json.dumps({'status':'pass','scenarios':len(records)}))
if __name__=='__main__': main()
