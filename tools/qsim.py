#!/usr/bin/env python3
"""What-if analysis on judged lines (no new model calls): qsim.py TAG
Simulates cheap fixes on the lines the panel already rated (median of three judges)."""
import collections, json, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from nlm_quality import load  # noqa: E402

items = [it for it in load(sys.argv[1]) if "sense" in it]
sp = [it for it in items if it["src"] == "student_pass"]


def rates(rs, name):
    n = len(rs)
    if not n:
        print(f"  {name:48s} n=0")
        return
    print(f"  {name:48s} n={n:4d}  sense {100 * sum(i['sense'] == 2 for i in rs) / n:3.0f}% / "
          f"nonsense {100 * sum(i['sense'] == 0 for i in rs) / n:3.0f}%")


print("shown model lines by the model's confidence (mean log-prob quartile):")
q = sorted(sp, key=lambda i: i["lp"])
for k in range(4):
    rates(q[k * len(q) // 4:(k + 1) * len(q) // 4], f"quartile {k + 1} (lp {q[k * len(q) // 4]['lp']:.2f}..)")
print("by weakest token (min log-prob) quartile:")
q = sorted(sp, key=lambda i: i["lp_min"])
for k in range(4):
    rates(q[k * len(q) // 4:(k + 1) * len(q) // 4], f"quartile {k + 1}")
print("by number of sentences:")
for n in (1, 2, 3):
    rates([i for i in sp if min(3, len(re.findall(r"[.!?~]+(\s|$)", i["raw"])) or 1) == n], f"{n}{'+' if n == 3 else ''} sentence(s)")
print("simulated fixes (per speaker/situation, from its 4 passing lines):")
by = collections.defaultdict(list)
for i in sp:
    by[(i["spk"], i["sit"])].append(i)
rates([max(v, key=lambda i: i["lp"]) for v in by.values()], "best of 4 by confidence")
rates([min(v, key=lambda i: (i["unseen_bigrams"], -i["lp"])) for v in by.values()], "best of 4 by fewest unseen pairs, then confidence")
rates([i for i in sp if i["unseen_bigrams"] == 0], "gate: no unseen word pairs")
rates([i for i in sp if i["unseen_bigrams"] == 0 and i["lp"] > -1.1], "gate: no unseen pairs and lp > -1.1")
rates([i for i in items if i["src"] == "teacher"], "teacher lines (control)")
