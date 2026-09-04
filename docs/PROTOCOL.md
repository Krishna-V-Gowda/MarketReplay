# Protocol contract

MarketReplay consumes a sequence of frames. Every frame begins with a two-byte unsigned big-endian payload length followed by exactly one ITCH message. The decoder supports the following TotalView-ITCH 5.0 layouts and rejects every other type.

| Type | Exact bytes | Parsed fields after common header |
|---|---:|---|
| S | 12 | event code |
| A | 36 | order reference, side, shares, stock, price |
| F | 40 | A fields; four-byte attribution is bounds-checked but does not affect book state |
| E | 31 | order reference, executed shares; match number is bounds-checked |
| C | 36 | E fields, printable flag, execution price; state reduction uses resting order |
| X | 23 | order reference, canceled shares |
| D | 19 | order reference |
| U | 35 | old reference, new reference, shares, price |

The common header contains message type (1), stock locate (2), tracking number (2), and a 48-bit nanoseconds-since-midnight timestamp (6). Integer fields are network byte order.

## Deliberate exclusions

Directory, imbalance, trade, cross-trade, broken-trade, retail-price-improvement, LULD, MWCB, and transport-session messages are not accepted. Supporting them would require semantics and tests rather than merely adding type labels.
