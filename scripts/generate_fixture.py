#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, json, random
from pathlib import Path

SYMBOLS = [(1, "ALPHA"), (2, "BETA"), (3, "GAMMA"), (4, "DELTA")]

def be(value: int, width: int) -> bytes: return value.to_bytes(width, "big")
def header(kind: str, locate: int, timestamp: int, tracking: int) -> bytes:
    return kind.encode() + be(locate, 2) + be(tracking & 0xFFFF, 2) + be(timestamp, 6)
def frame(payload: bytes) -> bytes: return be(len(payload), 2) + payload

def generate(events: int, seed: int) -> tuple[bytes, dict]:
    if events < 1: raise ValueError("events must be positive")
    rng = random.Random(seed); timestamp = 1; next_ref = 1; tracking = 1
    active: dict[int, dict] = {}; output = bytearray(); counts = {}
    def emit(kind: str, payload: bytes):
        nonlocal tracking
        output.extend(frame(payload)); counts[kind] = counts.get(kind, 0) + 1; tracking += 1
    emit("S", header("S", 0, timestamp, tracking) + b"O")
    while sum(counts.values()) < events:
        timestamp += rng.randint(1, 100)
        if not active or (len(active) < 64 and (len(active) < 32 or rng.random() < 0.44)):
            locate, stock = rng.choice(SYMBOLS); side = rng.choice("BS"); shares = rng.randint(1, 2000)
            price = rng.randint(5000, 25000); kind = "F" if rng.random() < 0.08 else "A"; ref = next_ref; next_ref += 1
            payload = header(kind, locate, timestamp, tracking) + be(ref, 8) + side.encode() + be(shares, 4) + stock.ljust(8).encode() + be(price, 4)
            if kind == "F": payload += b"TEST"
            active[ref] = {"locate": locate, "stock": stock, "side": side, "shares": shares, "price": price}
            emit(kind, payload); continue
        ref = rng.choice(tuple(active)); order = active[ref]; choice = rng.random()
        if choice < 0.35:
            qty = rng.randint(1, order["shares"]); kind = "C" if rng.random() < 0.12 else "E"
            payload = header(kind, order["locate"], timestamp, tracking) + be(ref, 8) + be(qty, 4) + be(tracking, 8)
            if kind == "C": payload += b"Y" + be(order["price"], 4)
            order["shares"] -= qty
            if order["shares"] == 0: del active[ref]
            emit(kind, payload)
        elif choice < 0.65:
            qty = rng.randint(1, order["shares"]); payload = header("X", order["locate"], timestamp, tracking) + be(ref, 8) + be(qty, 4)
            order["shares"] -= qty
            if order["shares"] == 0: del active[ref]
            emit("X", payload)
        elif choice < 0.82:
            payload = header("D", order["locate"], timestamp, tracking) + be(ref, 8); del active[ref]; emit("D", payload)
        else:
            new_ref = next_ref; next_ref += 1; new_shares = rng.randint(1, 2000); new_price = max(1, order["price"] + rng.randint(-50, 50))
            payload = header("U", order["locate"], timestamp, tracking) + be(ref, 8) + be(new_ref, 8) + be(new_shares, 4) + be(new_price, 4)
            active[new_ref] = {**order, "shares": new_shares, "price": new_price}; del active[ref]; emit("U", payload)
    data = bytes(output)
    manifest = {"events": events, "seed": seed, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(), "message_counts": dict(sorted(counts.items()))}
    return data, manifest

def main():
    p=argparse.ArgumentParser(); p.add_argument("--events",type=int,default=10000); p.add_argument("--seed",type=int,default=301); p.add_argument("--output",type=Path,required=True); p.add_argument("--manifest",type=Path)
    a=p.parse_args(); data, manifest=generate(a.events,a.seed); a.output.parent.mkdir(parents=True,exist_ok=True); a.output.write_bytes(data)
    if a.manifest: a.manifest.parent.mkdir(parents=True,exist_ok=True); a.manifest.write_text(json.dumps(manifest,indent=2,sort_keys=True)+"\n")
    print(json.dumps(manifest,sort_keys=True))
if __name__ == "__main__": main()
