# Verification methodology

Evidence is layered:

1. C++ unit tests cover exact layouts, valid transitions, invalid references, over-reduction, truncation, strict time, invariants, and deterministic serialization.
2. Python tests establish deterministic fixture generation and independent error handling.
3. Differential scenarios vary seeds and tape sizes and require exact canonical-state equality.
4. Mutation tests corrupt valid tapes by truncation, bit flips, insertion, and deletion. A valid rejection is acceptable; a signal, timeout, or unexplained exit is not.
5. Benchmarking occurs only after state identity. Raw repetitions, workload digest, compiler, Python version, machine, and limitations are retained.
6. Release packaging is checked again after fresh extraction.

This is strong empirical evidence for the documented subset, not formal verification or exhaustive fuzzing.
