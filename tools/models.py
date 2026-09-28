#!/usr/bin/env python3
"""List OpenRouter models matching the given words, with prices (no key needed): models.py word [word...]"""
import json, sys, urllib.request

data = json.load(urllib.request.urlopen("https://openrouter.ai/api/v1/models", timeout=60))["data"]
words = sys.argv[1:]
for m in sorted(data, key=lambda m: m["id"]):
    if any(w in m["id"] for w in words):
        p = m.get("pricing", {})
        print(f"{m['id']:48s} in ${float(p.get('prompt', 0)) * 1e6:6.3f}/M  out ${float(p.get('completion', 0)) * 1e6:6.3f}/M  "
              f"ctx {m.get('context_length')}")
