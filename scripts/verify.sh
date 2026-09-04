#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="${MARKETREPLAY_VERIFY_WORK:-$(mktemp -d "${TMPDIR:-/tmp}/marketreplay-verify.XXXXXX")}"
if [[ -z "${MARKETREPLAY_VERIFY_WORK:-}" ]]; then
  trap 'rm -rf "$WORK"' EXIT
fi
BUILD="$WORK/build"
OUT="$WORK/evidence"
RETAINED="$ROOT/evidence/verification"
mkdir -p "$OUT"
export PYTHONDONTWRITEBYTECODE=1

printf '%s\n' '[1/10] configure and build out of tree'
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$BUILD" --parallel >/dev/null

printf '%s\n' '[2/10] native C++ test suite'
ctest --test-dir "$BUILD" --output-on-failure

printf '%s\n' '[3/10] independent Python oracle tests'
python3 -m unittest discover -s "$ROOT/tests" -p 'test_*.py' -v

printf '%s\n' '[4/10] locked deterministic fixture and canonical identity'
python3 "$ROOT/scripts/generate_fixture.py" --events 10000 --seed 301 --output "$OUT/fixture.itch" --manifest "$OUT/fixture.json" >/dev/null
"$BUILD/marketreplay" "$OUT/fixture.itch" --json --strict-time --check-every 257 > "$OUT/cpp.json"
python3 "$ROOT/reference/oracle.py" "$OUT/fixture.itch" --strict-time --check-every 257 > "$OUT/python.json"
cmp "$OUT/cpp.json" "$OUT/python.json"

printf '%s\n' '[5/10] differential matrix'
python3 "$ROOT/scripts/differential.py" --binary "$BUILD/marketreplay" --scenarios 18 --output "$OUT/differential.json" >/dev/null

printf '%s\n' '[6/10] deterministic mutation containment'
python3 "$ROOT/scripts/robustness.py" --binary "$BUILD/marketreplay" --mutations 128 --output "$OUT/robustness.json" >/dev/null

printf '%s\n' '[7/10] repeated-report determinism'
"$BUILD/marketreplay" "$OUT/fixture.itch" --json > "$OUT/determinism-a.json"
"$BUILD/marketreplay" "$OUT/fixture.itch" --json > "$OUT/determinism-b.json"
cmp "$OUT/determinism-a.json" "$OUT/determinism-b.json"

printf '%s\n' '[8/10] retained evidence equivalence'
for file in fixture.itch fixture.json cpp.json python.json differential.json robustness.json determinism-a.json determinism-b.json; do
  cmp "$OUT/$file" "$RETAINED/$file"
done
printf 'all eight retained verification artifacts reproduced byte-for-byte\n'

printf '%s\n' '[9/10] Python compilation and first-party secret scan'
python3 - "$ROOT" <<'PY'
from pathlib import Path
import re, sys
root=Path(sys.argv[1])
paths=sorted((root/'reference').rglob('*.py'))+sorted((root/'scripts').rglob('*.py'))+sorted((root/'tests').rglob('*.py'))
for path in paths:
    compile(path.read_text(encoding='utf-8'), str(path), 'exec')
patterns=[
 re.compile(r'AKIA[0-9A-Z]{16}'),
 re.compile(r'gh[pousr]_[A-Za-z0-9_]{30,}'),
 re.compile(r'AIza[0-9A-Za-z_-]{30,}'),
 re.compile(r'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----'),
]
excluded={'.git','build','dist','__pycache__','.venv','venv','node_modules','.next'}
for path in root.rglob('*'):
    if not path.is_file() or any(part in excluded for part in path.parts):
        continue
    if path == root/'scripts'/'verify.sh' or path.stat().st_size > 5_000_000:
        continue
    try: text=path.read_text(encoding='utf-8')
    except (UnicodeDecodeError,OSError): continue
    if any(pattern.search(text) for pattern in patterns):
        raise SystemExit(f'secret-like material detected in {path.relative_to(root)}')
print(f'compiled {len(paths)} Python files; no high-confidence secret patterns detected')
PY

printf '%s\n' '[10/10] source-tree residue guard'
if find "$ROOT" -type d \( -name __pycache__ -o -name .pytest_cache -o -name .venv -o -name node_modules -o -name .next -o -name build -o -name dist \) -print -quit | grep -q .; then
  echo 'generated residue detected in source tree' >&2
  exit 1
fi
printf 'MarketReplay verification passed.\n'
