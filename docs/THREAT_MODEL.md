# Threat model

## Protected properties

- memory-safe bounded reads at the application layer;
- fail-closed handling of malformed or state-invalid messages;
- deterministic reconstruction independent of unordered-container iteration;
- no silent acceptance of unsupported message types;
- reproducible release contents.

## Adversarial inputs considered

Truncated prefixes and payloads, zero lengths, unknown types, incorrect exact lengths, duplicate references, unknown reductions, over-execution, replacement collisions, timestamp regression under strict policy, random bit flips, byte insertion, and byte deletion.

## Out of scope

Network denial of service, kernel or standard-library vulnerabilities, hostile multi-gigabyte files, exchange authentication, packet sequence recovery, and cryptographic tape authenticity.
