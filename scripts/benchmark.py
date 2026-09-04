#!/usr/bin/env python3
from __future__ import annotations
import argparse, json, platform, statistics, subprocess, sys, tempfile, time
from pathlib import Path
from generate_fixture import generate

def timed(command):
    start=time.perf_counter(); result=subprocess.run(command,capture_output=True,text=True,check=True); return time.perf_counter()-start,json.loads(result.stdout)

def main():
    p=argparse.ArgumentParser(); p.add_argument('--binary',type=Path,required=True); p.add_argument('--events',type=int,default=500000); p.add_argument('--seed',type=int,default=20260903); p.add_argument('--output',type=Path,required=True); p.add_argument('--markdown',type=Path,required=True); p.add_argument('--cpp-repetitions',type=int,default=7); p.add_argument('--python-repetitions',type=int,default=3); a=p.parse_args()
    root=Path(__file__).resolve().parents[1]; data,manifest=generate(a.events,a.seed)
    with tempfile.TemporaryDirectory() as td:
        fixture=Path(td)/'benchmark.itch'; fixture.write_bytes(data); cpp_cmd=[str(a.binary),str(fixture),'--json']; py_cmd=[sys.executable,str(root/'reference'/'oracle.py'),str(fixture)]
        for _ in range(2): timed(cpp_cmd)
        cpp=[timed(cpp_cmd) for _ in range(a.cpp_repetitions)]
        for _ in range(1): timed(py_cmd)
        py=[timed(py_cmd) for _ in range(a.python_repetitions)]
    cpp_reports={json.dumps(r,sort_keys=True) for _,r in cpp}; py_reports={json.dumps(r,sort_keys=True) for _,r in py}
    if len(cpp_reports)!=1 or len(py_reports)!=1 or next(iter(cpp_reports))!=next(iter(py_reports)): raise SystemExit('state identity prerequisite failed')
    cpp_times=[t for t,_ in cpp]; py_times=[t for t,_ in py]
    compiler=subprocess.run(['c++','--version'],capture_output=True,text=True).stdout.splitlines()[0]
    report={'schema_version':1,'workload':manifest,'environment':{'platform':platform.platform(),'machine':platform.machine(),'python':platform.python_version(),'compiler':compiler},'method':{'cpp_warmups':2,'cpp_repetitions':a.cpp_repetitions,'python_warmups':1,'python_repetitions':a.python_repetitions,'timer':'time.perf_counter wall clock','state_identity':'exact canonical JSON'},'cpp_seconds':cpp_times,'python_seconds':py_times,'cpp_median_seconds':statistics.median(cpp_times),'python_median_seconds':statistics.median(py_times),'cpp_median_events_per_second':a.events/statistics.median(cpp_times),'python_median_events_per_second':a.events/statistics.median(py_times),'median_speedup':statistics.median(py_times)/statistics.median(cpp_times),'fingerprint':cpp[0][1]['fingerprint'],'limitations':['Single machine and synthetic workload.','Includes process startup and file I/O.','Does not represent exchange certification, live feed handling, or universal throughput.']}
    a.output.parent.mkdir(parents=True,exist_ok=True); a.output.write_text(json.dumps(report,indent=2,sort_keys=True)+'\n')
    md=f"""# MarketReplay benchmark\n\n**Workload:** {a.events:,} deterministic synthetic ITCH messages (`sha256:{manifest['sha256']}`).  \n**Environment:** `{report['environment']['platform']}`; `{compiler}`; Python {platform.python_version()}.  \n**Method:** two C++ warmups then {a.cpp_repetitions} measured runs; one Python warmup then {a.python_repetitions} measured runs; wall-clock process + file I/O; exact canonical-state identity required before comparison.\n\n| implementation | median seconds | median messages/s |\n|---|---:|---:|\n| C++20 replay engine | {report['cpp_median_seconds']:.6f} | {report['cpp_median_events_per_second']:,.0f} |\n| independent Python oracle | {report['python_median_seconds']:.6f} | {report['python_median_events_per_second']:,.0f} |\n\nObserved median speed ratio on this retained workload: **{report['median_speedup']:.2f}×**. This is an environment-bound engineering measurement, not a universal language comparison.\n\n## Interpretation boundary\n\n- The input is synthetic and intentionally exercises the supported message subset.\n- The number includes process startup and file I/O; it is not per-message latency.\n- No live transport, packet loss, sequence recovery, exchange certification, or proprietary market feed was tested.\n- Raw repetitions and the exact state fingerprint are retained in `benchmark.json`.\n"""
    a.markdown.parent.mkdir(parents=True,exist_ok=True); a.markdown.write_text(md)
    print(json.dumps({'events':a.events,'cpp_median_events_per_second':report['cpp_median_events_per_second'],'python_median_events_per_second':report['python_median_events_per_second'],'median_speedup':report['median_speedup'],'fingerprint':report['fingerprint']},sort_keys=True))
if __name__=='__main__': main()
