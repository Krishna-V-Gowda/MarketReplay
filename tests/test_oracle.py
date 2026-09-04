import importlib.util, io, sys, unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from generate_fixture import generate
spec=importlib.util.spec_from_file_location('oracle',ROOT/'reference'/'oracle.py'); oracle=importlib.util.module_from_spec(spec); sys.modules['oracle']=oracle; spec.loader.exec_module(oracle)

class OracleTests(unittest.TestCase):
    def test_deterministic_fixture(self):
        a,ma=generate(1000,7); b,mb=generate(1000,7); self.assertEqual(a,b); self.assertEqual(ma,mb)
    def test_deterministic_replay(self):
        data,_=generate(500,8); self.assertEqual(oracle.replay_stream(io.BytesIO(data)),oracle.replay_stream(io.BytesIO(data)))
    def test_strict_time(self):
        data,_=generate(100,9); report=oracle.replay_stream(io.BytesIO(data),strict_time=True,check_every=7); self.assertEqual(report['total_frames'],100)
    def test_truncation_rejected(self):
        data,_=generate(100,9)
        with self.assertRaises(oracle.ReplayError): oracle.replay_stream(io.BytesIO(data[:-1]))
    def test_fingerprint_shape(self):
        data,_=generate(100,10); fp=oracle.replay_stream(io.BytesIO(data))['fingerprint']; self.assertEqual(len(fp),16); int(fp,16)

if __name__=='__main__': unittest.main()
