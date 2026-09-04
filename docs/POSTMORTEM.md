# Release postmortem: evidence without a portable artifact

## Failure

An early release summary described a working replay engine and its verification results, but the corresponding distribution did not include the complete source tree. The narrative existed; the independently runnable artifact did not.

That invalidated the release regardless of whether code had existed in an ephemeral workspace. A reviewer could not compile the engine, rerun the oracle comparison, inspect the parser, or verify the reported evidence.

## Root cause

Packaging was treated as clerical work after engineering rather than as part of the evidence graph. Reports and source were assembled through separate paths, so the release could retain downstream claims while omitting the artifact that supported them.

## Corrective design

The retained release now requires this chain to remain intact:

```text
source
  → configure + build
  → native tests
  → independent oracle
  → differential and mutation evidence
  → retained benchmark
  → archive membership
  → fresh extraction
  → rerun
```

The verification command builds out of tree, regenerates eight locked evidence artifacts in a temporary directory, and requires byte-for-byte equality with the retained copies. It never deletes the last-known-good evidence before a successful run.

The project also adopted a deliberately bounded C++20 implementation because the selected protocol subset, compiler, tests, and clean-room workflow could all be retained and reproduced together. Unsupported claims about a different implementation language or broader protocol coverage were removed.

## Lesson

A technical claim is portable only when its source, build inputs, tests, evidence, limits, and distribution travel together. Packaging is not downstream of correctness; it is one of the conditions under which correctness can be inspected.
