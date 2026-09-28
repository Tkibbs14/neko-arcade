#!/usr/bin/env python3
"""Free (no-model) metrics for a quality run: qfree.py TAG"""
import collections, json, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
items = json.load(open(os.path.join(ROOT, "build", "quality", sys.argv[1], "items.json")))
by = collections.defaultdict(list)
for it in items:
    by[it["src"]].append(it)
print("lines per source:", {k: len(v) for k, v in by.items()})
st = by["student_pass"] + by["student_fail"]
print(f"on-device pass rate: {len(by['student_pass'])}/{len(st)} = {100 * len(by['student_pass']) / len(st):.1f}%")
print(f"verbatim teacher copies among shown model lines: {sum(i['copy'] for i in by['student_pass'])}/{len(by['student_pass'])}")
for s in ("teacher", "student_pass", "student_fail"):
    c = collections.Counter(min(3, i["unseen_bigrams"]) for i in by[s])
    n = len(by[s])
    print(f"unseen word pairs, {s:13s}: " + "  ".join(f"{k if k < 3 else '3+'}: {100 * c[k] / n:4.1f}%" for k in range(4)))
    print(f"   mean length {sum(i['chars'] for i in by[s]) / n:.0f} chars")

# how {item} is introduced: the word before it
def before(raw):
    m = re.search(r"(\S+)\s+\{item\}", raw)
    return m.group(1).lower().strip(".,!?") if m else ("<start>" if raw.startswith("{item}") else None)

for s in ("teacher", "student_pass"):
    c = collections.Counter(before(i["raw"]) for i in by[s] if "{item}" in i["raw"])
    tot = sum(c.values())
    print(f"word before {{item}}, {s}: " + ", ".join(f"{w} {100 * n / tot:.0f}%" for w, n in c.most_common(12)))
