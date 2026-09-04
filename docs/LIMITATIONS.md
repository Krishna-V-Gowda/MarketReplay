# Limitations

See the README for user-facing scope. Additional engineering limitations:

- the entire payload for each frame is allocated before decoding;
- active orders use general-purpose hash tables and books use tree maps rather than a cache-optimized arena;
- validation is O(number of active orders) and should not run every message in high-volume workloads;
- the CLI is single-threaded by design, preserving deterministic transition order;
- no snapshot or resume format is defined in v1.0.0;
- F and C fields not required for state reconstruction are bounds-checked but not retained in the report.
