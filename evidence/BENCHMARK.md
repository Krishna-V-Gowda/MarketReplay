# MarketReplay benchmark

**Workload:** 500,000 deterministic synthetic ITCH messages (`sha256:f180927301413d769849f20cc7aca9ea9032e152f6a92770f7e3c1886871f21f`).  
**Environment:** `Linux-6.18.35-x86_64-with-glibc2.41`; `c++ (Debian 14.2.0-19) 14.2.0`; Python 3.13.5.  
**Method:** two C++ warmups then 7 measured runs; one Python warmup then 3 measured runs; wall-clock process + file I/O; exact canonical-state identity required before comparison.

| implementation | median seconds | median messages/s |
|---|---:|---:|
| C++20 replay engine | 0.124657 | 4,010,991 |
| independent Python oracle | 1.778142 | 281,192 |

Observed median speed ratio on this retained workload: **14.26×**. This is an environment-bound engineering measurement, not a universal language comparison.

## Interpretation boundary

- The input is synthetic and intentionally exercises the supported message subset.
- The number includes process startup and file I/O; it is not per-message latency.
- No live transport, packet loss, sequence recovery, exchange certification, or proprietary market feed was tested.
- Raw repetitions and the exact state fingerprint are retained in `benchmark.json`.
