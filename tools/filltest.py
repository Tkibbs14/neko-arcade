#!/usr/bin/env python3
"""The device's placeholder fill (nlm_fill in src/nekolm.c) must equal the panel's copy (c_fill in nlm_quality.py)
on every line the game can compose: every approved pair and every whole unit, with every value the game uses."""
import json, os, subprocess, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from nlm_quality import FILL, c_fill  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KEYS = {1: "item", 2: "n", 3: "dir"}
bank = json.load(open(os.path.join(ROOT, "build", "compose_bank.json")))
cases = []
for p in bank:
    if not p["key"]:
        continue
    key = KEYS[p["key"]]
    values = next((v for pre, v in FILL.items() if p["sit"].startswith(pre)), [str(n) for n in range(2, 19)])
    texts = [u[1] for u in p["op"] + p["rest"] if "{" in u[1]]
    texts += [p["op"][o][1] + " " + p["rest"][r][1] for o, r in p["ok"]]
    for t in texts:
        for v in values:
            cases.append((key, v, t))
inp = "".join(f"{k}\t{v}\t{t}\n" for k, v, t in cases)
out = subprocess.run([os.path.join(ROOT, "build", "fill_test")], input=inp, capture_output=True, text=True,
                     check=True).stdout.split("\n")
bad = [(t, v, o, c_fill(t, v)) for (k, v, t), o in zip(cases, out) if o != c_fill(t, v)]
changed = sum(c_fill(t, v) != c_fill(t, v, articles=False) for k, v, t in cases)
print(f"filltest: {len(cases)} lines, {len(bad)} differ between C and Python; the article fix changes {changed}")
for t, v, o, c in bad[:10]:
    print(f"  {v!r} in {t!r}: C {o!r} / Python {c!r}")
sys.exit(1 if bad else 0)
