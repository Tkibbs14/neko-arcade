#!/usr/bin/env python3
"""Second-generation teacher lines, written with the guide in tools/distill_spec2.py.

  distill_gen2.py MODEL OUTDIR N ROUND [SPK/SIT,...]
    MODEL   the teacher, pinned (for example deepseek/deepseek-v4.1-flash)
    OUTDIR  round R writes OUTDIR/rR/<speaker>__<situation>.json
    N       lines kept per combination in this round
    ROUND   1, 2, ...: a later round sees the earlier rounds' lines and must not repeat them
    SPK/SIT only these combinations (a pilot)

A line is kept when it passes the first generation's checks (length, characters, placeholder) and also
recombines: it splits into a first sentence and the rest exactly as tools/mkcompose.py splits lines, with the
placeholder in the first sentence. Resumable: combinations already written are skipped."""
import concurrent.futures as cf, json, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ai import chat, spent  # noqa: E402
from distill_spec2 import RULES, SITS, VOICES, WRITER, place_note  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PLACEHOLDER = re.compile(r"\{(\w+)\}")
ALLOWED = re.compile(r"^[A-Za-z0-9 .,!?'~\-{}:;()]+$")
SPLIT = re.compile(r"(?<!Mr\.)(?<!Mrs\.)(?<!Ms\.)(?<=[.!?~])\s+(?=[A-Z\"'])")     # as in tools/mkcompose.py
SPLIT_LOWER = re.compile(r"(?<=[.!?~])\s+(?=[A-Za-z\"'])")
BACKREF = re.compile(r"^(it|that|this|he|she|they|there|then|too|also|again|so|but)\b", re.I)


def clean(line, desc):
    """The first generation's checks (tools/distill_gen.py), unchanged."""
    line = line.strip().strip('"').strip()
    line = line.replace("’", "'").replace("‘", "'").replace("“", '"').replace("”", '"')
    line = line.replace("…", "...").replace("—", " - ").replace("–", "-").replace('"', "'")
    line = re.sub(r"\s+", " ", line)
    if not (12 <= len(line) <= 84) or not ALLOWED.match(line) or "*" in line:
        return None
    if set(PLACEHOLDER.findall(desc)) != set(PLACEHOLDER.findall(line)):
        return None
    return line


def recombines(line, spk, desc):
    parts = (SPLIT_LOWER if spk == "KITTEN" else SPLIT).split(line)
    if len(parts) < 2:
        return False
    rest = " ".join(parts[1:])
    return not (PLACEHOLDER.search(desc) and ("{" not in parts[0] or "{" in rest))


def generate(model, outdir, n, rnd, spk, sit, desc):
    path = os.path.join(outdir, f"r{rnd}", f"{spk}__{sit}.json")
    if os.path.exists(path):
        return spk, sit, len(json.load(open(path))), 0, 0, 0
    earlier = []
    for r in range(1, rnd):
        p = os.path.join(outdir, f"r{r}", f"{spk}__{sit}.json")
        if os.path.exists(p):
            earlier += json.load(open(p))
    system = ("You write short spoken lines for characters in a cozy pixel-art game. " + RULES +
              '\nReply with JSON only: {"lines": ["...", "..."]}')
    note = place_note(sit)
    user = (f"Speaker:\n{VOICES[spk]}\nHow they talk:\n{WRITER[spk]}\n\nSituation: {desc}\n" + (note + "\n" if note else "") +
            f"\nWrite {n + 15} different lines this speaker says in this situation, each under 80 characters.")
    if earlier:
        user += ("\n\nThese lines already exist. Do not repeat or closely paraphrase them: use different wording, "
                 "sentence shapes, lengths and angles on the situation.\n" + "\n".join(earlier))
    examples = {e.strip().lower() for e in re.findall(r'"([^"]{8,})"', WRITER[spk] + VOICES[spk])}   # no verbatim copies
    lines, invalid, single = [], 0, 0
    for attempt in range(3):
        try:
            raw = chat(system, user, json_mode=True, temperature=1.0, max_tokens=6000, model=model,
                       tag=f"v2/{spk}/{sit}/r{rnd}")
            got = json.loads(raw).get("lines", [])
        except (Exception, SystemExit):
            got = []
        seen = {l.lower() for l in lines + earlier} | examples
        for g in got:
            c = clean(str(g), desc)
            if not c or c.lower() in seen:
                invalid += 1
            elif not recombines(c, spk, desc):
                single += 1
            else:
                lines.append(c)
                seen.add(c.lower())
        if len(lines) >= n * 0.8:
            break
    lines = lines[:n]
    os.makedirs(os.path.dirname(path), exist_ok=True)
    json.dump(lines, open(path, "w", encoding="utf-8"), indent=0, ensure_ascii=True)
    backrefs = sum(bool(BACKREF.match(" ".join((SPLIT_LOWER if spk == "KITTEN" else SPLIT).split(l)[1:]))) for l in lines)
    return spk, sit, len(lines), invalid, single, backrefs


def main():
    model, outdir, n, rnd = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
    outdir = outdir if os.path.isabs(outdir) else os.path.join(ROOT, outdir)
    only = set(sys.argv[5].split(",")) if len(sys.argv) > 5 else None
    todo = [(s, t, d) for s, t, d in SITS if not only or f"{s}/{t}" in only]
    print(f"gen2: {model}, {len(todo)} combinations, {n} lines each, round {rnd}; spend so far ${spent():.4f}", flush=True)
    kept = inv = sing = back = 0
    with cf.ThreadPoolExecutor(max_workers=8) as pool:
        for spk, sit, k, i, s1, b in pool.map(lambda a: generate(model, outdir, n, rnd, *a), todo):
            kept, inv, sing, back = kept + k, inv + i, sing + s1, back + b
            print(f"  {spk:12s} {sit:20s} {k:3d} kept, {i} invalid or repeated, {s1} single-sentence, "
                  f"{b} kept rests start with a back-reference", flush=True)
    print(f"gen2: {kept} kept, {inv} invalid or repeated, {sing} single-sentence dropped, {back} kept lines whose rest "
          f"starts with a back-reference word; spend so far ${spent():.4f}")


if __name__ == "__main__":
    main()
