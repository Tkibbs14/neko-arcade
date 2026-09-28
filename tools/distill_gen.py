#!/usr/bin/env python3
"""Teacher side of the Neko Arcade distillation: have a large model write short in-character lines for
every (speaker, situation) the game uses, filter them, and write the training set for the tiny in-game
model.

Output:
  content/distill/raw/<speaker>__<situation>.json   every accepted line per combination (round 1)
  content/distill/raw2/...                           round 2: fresh lines, written with round 1 in view
  content/distill/dataset.txt                        one example per line: <spk><sit>text  (tag bytes >= 0x80)
  content/distill/tags.json                          speaker/situation byte codes (shared with the game)
Usage: distill_gen.py [N_PER_COMBO] [ROUND]   (re-running only fills combinations that are missing)
"""
import concurrent.futures as cf, json, os, re, sys, threading

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ai import chat, spent  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "content", "distill")
RAW = os.path.join(OUT, "raw")
N = int(sys.argv[1]) if len(sys.argv) > 1 else 60
ROUND = int(sys.argv[2]) if len(sys.argv) > 2 else 1
ROUND_DIRS = [RAW, os.path.join(OUT, "raw2")]

from distill_spec import RULES, VOICES, SITS  # noqa: E402

PLACEHOLDER = re.compile(r"\{(\w+)\}")
ALLOWED = re.compile(r"^[A-Za-z0-9 .,!?'~\-{}:;()]+$")


def clean(line, sit_desc):
    line = line.strip().strip('"').strip()
    line = line.replace("’", "'").replace("‘", "'").replace("“", '"').replace("”", '"')
    line = line.replace("…", "...").replace("—", " - ").replace("–", "-").replace('"', "'")
    line = re.sub(r"\s+", " ", line)
    if not (12 <= len(line) <= 84) or not ALLOWED.match(line) or "*" in line:
        return None
    need = set(PLACEHOLDER.findall(sit_desc))
    have = set(PLACEHOLDER.findall(line))
    if need != have:
        return None
    return line


lock = threading.Lock()


def generate(spk, sit, desc):
    path = os.path.join(ROUND_DIRS[ROUND - 1], f"{spk}__{sit}.json")
    if os.path.exists(path):
        return spk, sit, len(json.load(open(path))), 0
    earlier = []
    for d in ROUND_DIRS[:ROUND - 1]:
        p = os.path.join(d, f"{spk}__{sit}.json")
        if os.path.exists(p):
            earlier += json.load(open(p))
    system = ("You write short spoken lines for characters in a cozy pixel-art game. " + RULES +
              '\nReply with JSON only: {"lines": ["...", "..."]}')
    user = (f"Speaker:\n{VOICES[spk]}\n\nSituation: {desc}\n\nWrite {N + 12} different lines this speaker says in "
            f"this situation.")
    if earlier:
        user += ("\n\nThese lines already exist. Do not repeat or closely paraphrase them: use different wording, "
                 "sentence shapes, lengths and angles on the situation.\n" + "\n".join(earlier))
    lines, dropped = [], 0
    for attempt in range(2):
        try:
            raw = chat(system, user, json_mode=True, temperature=1.0, max_tokens=4000, tag=f"{spk}/{sit}/r{ROUND}")
            got = json.loads(raw).get("lines", [])
        except Exception as e:  # malformed JSON: try once more
            got = []
        seen = {l.lower() for l in lines + earlier}
        for g in got:
            c = clean(str(g), desc)
            if c and c.lower() not in seen:
                lines.append(c)
                seen.add(c.lower())
            else:
                dropped += 1
        if len(lines) >= N * 0.8:
            break
    lines = lines[:N]
    with lock:
        json.dump(lines, open(path, "w", encoding="utf-8"), indent=0, ensure_ascii=True)
    return spk, sit, len(lines), dropped


os.makedirs(ROUND_DIRS[ROUND - 1], exist_ok=True)
speakers = sorted({s for s, _, _ in SITS})
situations = sorted({t for _, t, _ in SITS})
tags = {"speakers": {s: 0x80 + i for i, s in enumerate(speakers)},
        "situations": {t: 0xA0 + i for i, t in enumerate(situations)}}
assert 0x80 + len(speakers) <= 0xA0 and 0xA0 + len(situations) <= 0x100
json.dump(tags, open(os.path.join(OUT, "tags.json"), "w"), indent=1)

with cf.ThreadPoolExecutor(max_workers=8) as pool:
    for spk, sit, n, dropped in pool.map(lambda a: generate(*a), SITS):
        print(f"{spk:12s} {sit:20s} {n:3d} kept, {dropped} dropped", flush=True)

total = 0
with open(os.path.join(OUT, "dataset.txt"), "wb") as f:
    for spk, sit, _ in SITS:
        seen = set()
        for d in ROUND_DIRS:
            p = os.path.join(d, f"{spk}__{sit}.json")
            for line in json.load(open(p)) if os.path.exists(p) else []:
                if line.lower() in seen:
                    continue
                seen.add(line.lower())
                f.write(bytes([tags["speakers"][spk], tags["situations"][sit]]) + line.encode("ascii") + b"\n")
                total += 1
print(f"dataset: {total} lines over {len(SITS)} combinations; OpenRouter spend so far ${spent():.4f}")
