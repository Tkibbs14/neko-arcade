#!/usr/bin/env python3
"""Peek at judge output for a quality run: qpeek.py TAG — per judge file, what was served and how complete it is."""
import glob, json, os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
for path in sorted(glob.glob(os.path.join(ROOT, "build", "quality", sys.argv[1], "judge_*.jsonl"))):
    recs = [json.loads(l) for l in open(path)]
    rated = sum(len(r["ratings"]) for r in recs)
    missing = sum(len(r["missing"]) for r in recs)
    providers = sorted({str(r["provider"]) for r in recs})
    print(f"{os.path.basename(path)}: {len(recs)} batches, {rated} rated, {missing} missing, served by {providers}, "
          f"reasoning kept in {sum(bool(r['reasoning']) for r in recs)}/{len(recs)} batches")
    if recs and recs[0]["ratings"]:
        print("   e.g.", json.dumps(recs[0]["ratings"][0]))
