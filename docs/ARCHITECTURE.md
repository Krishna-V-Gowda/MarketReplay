# Architecture

The decoder and transition engine are separate. `parse_message` accepts one already bounded frame and produces a typed event. `ReplayEngine::apply` owns state transitions. This keeps byte-layout defects distinct from order-state defects.

Active orders are indexed by reference. Per-symbol bid and ask aggregates are updated in the same transition that mutates the order index. `validate` independently reconstructs every level from active orders and compares the result with maintained aggregates. Checks can run periodically and always run at end-of-file.

Canonical output sorts message types, symbols, prices, and order references. Both implementations serialize the same semantic state and compute the same non-cryptographic FNV-1a state identifier. Differential equality is over the full canonical JSON, not only the identifier.

The Python oracle does not call the C++ library or share transition code. Shared protocol constants are intentionally duplicated so disagreement can expose implementation errors; the specification and generated fixtures are the common contract.
