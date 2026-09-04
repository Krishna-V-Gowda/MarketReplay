#!/usr/bin/env python3
"""Independent Python reference model for the documented MarketReplay subset."""
from __future__ import annotations

import argparse
import json
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import BinaryIO

LENGTHS = {"S": 12, "A": 36, "F": 40, "E": 31, "C": 36, "X": 23, "D": 19, "U": 35}
MASK64 = (1 << 64) - 1

class ReplayError(ValueError):
    pass

def u16(data: bytes, offset: int) -> int: return int.from_bytes(data[offset:offset+2], "big")
def u32(data: bytes, offset: int) -> int: return int.from_bytes(data[offset:offset+4], "big")
def u48(data: bytes, offset: int) -> int: return int.from_bytes(data[offset:offset+6], "big")
def u64(data: bytes, offset: int) -> int: return int.from_bytes(data[offset:offset+8], "big")

def fnv1a64(value: str) -> str:
    result = 14695981039346656037
    for byte in value.encode():
        result ^= byte
        result = (result * 1099511628211) & MASK64
    return f"{result:016x}"

@dataclass
class Order:
    reference: int
    stock_locate: int
    stock: str
    side: str
    shares: int
    price: int
    timestamp: int

class Engine:
    def __init__(self, strict_time: bool = False, check_every: int = 0):
        self.strict_time = strict_time
        self.check_every = check_every
        self.orders: dict[int, Order] = {}
        self.counts: dict[str, int] = {}
        self.total_frames = 0
        self.last_timestamp = 0
        self.executed_shares = 0
        self.canceled_shares = 0
        self.replaced_orders = 0

    def apply(self, event: dict) -> None:
        timestamp = event["timestamp"]
        if self.strict_time and self.total_frames and timestamp < self.last_timestamp:
            raise ReplayError("timestamp regression under strict-time policy")
        self.last_timestamp = max(self.last_timestamp, timestamp)
        kind = event["type"]
        self.counts[kind] = self.counts.get(kind, 0) + 1
        self.total_frames += 1
        if kind in {"A", "F"}: self.add(event)
        elif kind in {"E", "C"}: self.reduce(event["order_ref"], event["shares"], True)
        elif kind == "X": self.reduce(event["order_ref"], event["shares"], False)
        elif kind == "D": self.delete(event["order_ref"])
        elif kind == "U": self.replace(event)
        if self.check_every and self.total_frames % self.check_every == 0: self.validate()

    def add(self, event: dict) -> None:
        ref, shares, price, side = event["order_ref"], event["shares"], event["price"], event["side"]
        if not ref or not shares or not price: raise ReplayError("add order requires nonzero reference, shares, and price")
        if side not in {"B", "S"}: raise ReplayError("add order side must be B or S")
        if ref in self.orders: raise ReplayError("duplicate active order reference")
        self.orders[ref] = Order(ref, event["stock_locate"], event["stock"], side, shares, price, event["timestamp"])

    def reduce(self, ref: int, shares: int, execution: bool) -> None:
        if shares <= 0: raise ReplayError("order reduction must be positive")
        if ref not in self.orders: raise ReplayError("order reduction references unknown active order")
        order = self.orders[ref]
        if shares > order.shares: raise ReplayError("order reduction exceeds remaining shares")
        if execution: self.executed_shares += shares
        else: self.canceled_shares += shares
        order.shares -= shares
        if order.shares == 0: del self.orders[ref]

    def delete(self, ref: int) -> None:
        if ref not in self.orders: raise ReplayError("delete references unknown active order")
        del self.orders[ref]

    def replace(self, event: dict) -> None:
        old, new, shares, price = event["order_ref"], event["new_order_ref"], event["shares"], event["price"]
        if not new or not shares or not price: raise ReplayError("replace requires nonzero new reference, shares, and price")
        if old not in self.orders: raise ReplayError("replace references unknown active order")
        if new in self.orders: raise ReplayError("replace new reference is already active")
        previous = self.orders.pop(old)
        self.orders[new] = Order(new, previous.stock_locate, previous.stock, previous.side, shares, price, event["timestamp"])
        self.replaced_orders += 1

    def books(self) -> dict[str, dict[str, list[list[int]]]]:
        aggregate: dict[str, dict[str, dict[int, int]]] = {}
        for order in self.orders.values():
            book = aggregate.setdefault(order.stock, {"asks": {}, "bids": {}})
            side = "bids" if order.side == "B" else "asks"
            book[side][order.price] = book[side].get(order.price, 0) + order.shares
        result = {}
        for stock in sorted(aggregate):
            book = aggregate[stock]
            result[stock] = {
                "asks": [[p, book["asks"][p]] for p in sorted(book["asks"])],
                "bids": [[p, book["bids"][p]] for p in sorted(book["bids"], reverse=True)],
            }
        return result

    def validate(self) -> None:
        for ref, order in self.orders.items():
            if ref != order.reference or order.shares <= 0 or order.price <= 0 or order.side not in {"B", "S"}:
                raise ReplayError("active-order invariant failed")
        self.books()

    def state_text(self) -> str:
        parts = [f"frames={self.total_frames}", f"last={self.last_timestamp}", f"executed={self.executed_shares}",
                 f"canceled={self.canceled_shares}", f"replaced={self.replaced_orders}"]
        value = ";".join(parts) + ";"
        for kind in sorted(self.counts): value += f"m{kind}={self.counts[kind]};"
        for ref in sorted(self.orders):
            o = self.orders[ref]
            value += f"o={o.reference},{o.stock_locate},{o.stock},{o.side},{o.shares},{o.price},{o.timestamp};"
        return value

    def report(self) -> dict:
        self.validate()
        orders = [self.orders[ref].__dict__.copy() for ref in sorted(self.orders)]
        return {"active_orders": len(orders), "books": self.books(), "canceled_shares": self.canceled_shares,
                "executed_shares": self.executed_shares, "fingerprint": fnv1a64(self.state_text()),
                "last_timestamp": self.last_timestamp, "message_counts": dict(sorted(self.counts.items())),
                "orders": orders, "replaced_orders": self.replaced_orders, "total_frames": self.total_frames}

def parse_message(message: bytes) -> dict:
    if not message: raise ReplayError("zero-length ITCH message")
    kind = chr(message[0])
    if kind not in LENGTHS: raise ReplayError(f"unsupported ITCH message type: {kind}")
    if len(message) != LENGTHS[kind]: raise ReplayError(f"message {kind} has length {len(message)}; expected {LENGTHS[kind]}")
    event = {"type": kind, "stock_locate": u16(message, 1), "timestamp": u48(message, 5)}
    if kind == "S": event["system_event_code"] = chr(message[11])
    elif kind in {"A", "F"}:
        event.update(order_ref=u64(message, 11), side=chr(message[19]), shares=u32(message, 20),
                     stock=message[24:32].decode("ascii").rstrip(" "), price=u32(message, 32))
        if not event["stock"]: raise ReplayError("stock symbol is empty")
    elif kind in {"E", "C", "X"}: event.update(order_ref=u64(message, 11), shares=u32(message, 19))
    elif kind == "D": event["order_ref"] = u64(message, 11)
    elif kind == "U": event.update(order_ref=u64(message, 11), new_order_ref=u64(message, 19), shares=u32(message, 27), price=u32(message, 31))
    return event

def replay_stream(stream: BinaryIO, strict_time: bool = False, check_every: int = 0) -> dict:
    engine = Engine(strict_time, check_every)
    while True:
        prefix = stream.read(2)
        if not prefix: break
        if len(prefix) != 2: raise ReplayError("truncated two-byte frame length")
        length = int.from_bytes(prefix, "big")
        if length == 0: raise ReplayError("zero-length frame")
        payload = stream.read(length)
        if len(payload) != length: raise ReplayError("truncated ITCH frame payload")
        engine.apply(parse_message(payload))
    return engine.report()

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("--strict-time", action="store_true")
    parser.add_argument("--check-every", type=int, default=0)
    args = parser.parse_args()
    try:
        with args.input.open("rb") as handle:
            print(json.dumps(replay_stream(handle, args.strict_time, args.check_every), sort_keys=True, separators=(",", ":")))
        return 0
    except (OSError, ReplayError) as error:
        print(f"replay error: {error}", file=__import__("sys").stderr)
        return 3

if __name__ == "__main__": raise SystemExit(main())
