#!/usr/bin/env python3
"""Whisker Detective case factory: the teacher writes small apartment-building mysteries as JSON; a
deterministic checker rejects any case whose logic does not hold together; survivors are baked into
src/gen/cases_data.c.

  cases_gen.py gen N     ask for N cases (kept ones accumulate in content/cases/cases.json)
  cases_gen.py bake      write src/gen/cases_data.c from content/cases/cases.json
"""
import json, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ai import chat, spent  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIR = os.path.join(ROOT, "content", "cases")
DB = os.path.join(DIR, "cases.json")
SUSPECTS = ("NIA", "MAKO", "MOCHA")

CAST = """Setting: a small, cozy apartment building where cat demi-humans live. Everyone is an adult and a friend;
the "crimes" are harmless, funny and wholesome (a missing pudding, a moved plant, a mystery sound, a borrowed item).
Suspects (always exactly these three):
- NIA: Nia Takahashi, 24, black-cat indie game developer, dry and precise, lives on energy drinks and ramen,
  works until 2 am with headphones on.
- MAKO: Mako Shirase, 20, magenta-haired, fiercely competitive, keeps score of everything, hates losing,
  bakes to prove a point.
- MOCHA: Mocha, 27, caramel-haired catgirl who owns the cafe downstairs, brisk and warm, up at 5 am,
  makes coffee puns.
The detective is Shio Minase, 22, a precise forensic analyst with glasses, and the player is her partner."""

SCHEMA = """Reply with JSON only, in exactly this shape:
{
 "title": "short case title (max 24 chars)",
 "intro": "what happened, as Shio would brief it (max 150 chars)",
 "culprit": "NIA" | "MAKO" | "MOCHA",
 "clues": [ {"id": "one_word", "name": "short clue name (max 16 chars)", "desc": "what you see (max 80 chars)"} ],
 "statements": [ {"who": "NIA" | "MAKO" | "MOCHA", "text": "what they claim (max 80 chars)", "contradicted_by": "clue id or null"} ],
 "confession": "the culprit admits it, in their own voice (max 120 chars)",
 "wrapup": "Shio's closing line (max 100 chars)"
}
Logic rules (they are checked by code):
- 3 or 4 clues. 6 to 9 statements, 2 or 3 per suspect.
- Exactly ONE statement is contradicted by a clue, it is spoken by the culprit, and that clue alone proves the
  culprit lied; set its contradicted_by to that clue's id. Every other statement has contradicted_by null.
- One other clue is a red herring that seems to point at an innocent suspect but is explained by their statements.
- The contradiction must be fair: a careful player can see it from the clue text and the statement text alone.
- Wholesome, funny, no romance. Plain ASCII only (no emoji, no curly quotes)."""


def ascii_ok(s, n):
    return isinstance(s, str) and 3 <= len(s) <= n and all(32 <= ord(ch) < 127 for ch in s) and "*" not in s


def check(c):
    """Return a list of problems (empty = case accepted)."""
    p = []
    if not ascii_ok(c.get("title"), 24): p.append("title")
    if not ascii_ok(c.get("intro"), 150): p.append("intro")
    if c.get("culprit") not in SUSPECTS: p.append("culprit")
    clues = c.get("clues") or []
    if not 3 <= len(clues) <= 4: p.append("clue count")
    ids = set()
    for cl in clues:
        if not re.fullmatch(r"[a-z_]{2,16}", str(cl.get("id", ""))): p.append("clue id")
        if not ascii_ok(cl.get("name"), 16): p.append("clue name")
        if not ascii_ok(cl.get("desc"), 80): p.append("clue desc")
        ids.add(cl.get("id"))
    sts = c.get("statements") or []
    if not 6 <= len(sts) <= 9: p.append("statement count")
    per = {s: 0 for s in SUSPECTS}
    contra = []
    for st in sts:
        if st.get("who") not in SUSPECTS: p.append("statement who"); continue
        per[st["who"]] += 1
        if not ascii_ok(st.get("text"), 80): p.append("statement text")
        if st.get("contradicted_by") is not None:
            contra.append(st)
    if any(not 2 <= n <= 3 for n in per.values()): p.append("statements per suspect")
    if len(contra) != 1: p.append("need exactly one contradiction")
    elif contra[0]["who"] != c.get("culprit"): p.append("contradiction not by culprit")
    elif contra[0]["contradicted_by"] not in ids: p.append("contradiction clue missing")
    if not ascii_ok(c.get("confession"), 120): p.append("confession")
    if not ascii_ok(c.get("wrapup"), 100): p.append("wrapup")
    return p


def gen(n):
    os.makedirs(DIR, exist_ok=True)
    db = json.load(open(DB)) if os.path.exists(DB) else []
    culprits = [c["culprit"] for c in db]
    tried = 0
    while len(db) < n and tried < n * 3:
        tried += 1
        want = min(SUSPECTS, key=culprits.count)          # keep the culprits balanced
        titles = ", ".join(c["title"] for c in db) or "none yet"
        raw = chat("You design fair, tiny whodunit puzzles for a cozy game.\n" + CAST + "\n\n" + SCHEMA,
                   f"Write one new case. The culprit must be {want}. Avoid repeating these cases: {titles}.",
                   json_mode=True, temperature=1.0, max_tokens=2500, tag="case")
        try:
            c = json.loads(raw)
        except Exception:
            print("rejected: not JSON")
            continue
        problems = check(c)
        if problems:
            print(f"rejected '{c.get('title')}': {', '.join(sorted(set(problems)))}")
            continue
        db.append(c)
        culprits.append(c["culprit"])
        json.dump(db, open(DB, "w"), indent=1)
        print(f"kept {len(db)}: {c['title']} (culprit {c['culprit']})")
    print(f"{len(db)} cases; OpenRouter spend so far ${spent():.4f}")


def cstr(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


# Kitchen spots, in the order of the SPOT_* enum in src/cases.h. A clue goes where its description first
# names a place ("next to the fridge", "in the trash"); failing that, where its object usually lives.
SPOTS = ["fridge", "pantry", "counter", "sink", "stove", "table", "chair", "couch", "trash", "floor", "door"]
PLACE_WORDS = {"fridge": "fridge", "pantry": "pantry", "counter": "counter", "sink": "sink", "oven": "stove",
               "stove": "stove", "table": "table", "chair": "chair", "couch": "couch", "sofa": "couch",
               "trash": "trash", "bin": "trash", "floor": "floor", "door": "door"}
OBJECT_WORDS = {"scale": "counter", "controller": "couch", "apron": "chair", "receipt": "table",
                "wrapper": "trash", "carton": "trash", "footprint": "floor", "timer": "stove"}


def clue_spot(cl):
    for text, words in ((cl["desc"], PLACE_WORDS), (cl["name"] + " " + cl["desc"], OBJECT_WORDS)):
        hits = [(m.start(), words[w]) for w in words for m in re.finditer(r"\b" + w, text.lower())]
        if hits:
            return SPOTS.index(min(hits)[1])
    return SPOTS.index("table")


def unquote_shio(s):
    """Intros and wrap-ups are spoken by Shio, so drop a narrated "Shio briefs: '...'" wrapper."""
    m = re.fullmatch(r"Shio(?: briefs)?: '(.*)'", s)
    return m.group(1) if m else s


def bake():
    db = json.load(open(DB))
    who = {"NIA": 0, "MAKO": 1, "MOCHA": 3}          # cast indices (CH_NIA, CH_MAKO, CH_MOCHA)
    out = ["/* Generated by tools/cases_gen.py bake - AI-written cases that passed the logic checker. */\n",
           '#include "../cases.h"\n\n']
    for i, c in enumerate(db):
        ids = [cl["id"] for cl in c["clues"]]
        out.append(f"static const Clue clues_{i}[] = {{\n")
        out += [f"    {{ {cstr(cl['name'])}, {cstr(cl['desc'])}, SPOT_{SPOTS[clue_spot(cl)].upper()} }},\n"
                for cl in c["clues"]]
        out.append("};\n")
        out.append(f"static const Statement statements_{i}[] = {{\n")
        for st in c["statements"]:
            k = ids.index(st["contradicted_by"]) if st.get("contradicted_by") else -1
            out.append(f"    {{ {who[st['who']]}, {cstr(st['text'])}, {k} }},\n")
        out.append("};\n")
    out.append("\nconst Case cases[] = {\n")
    for i, c in enumerate(db):
        place = "PLACE_CAFE" if re.search(r"\bcafe\b", c["intro"], re.I) else "PLACE_APARTMENT"
        out.append(f"    {{ {cstr(c['title'])}, {cstr(unquote_shio(c['intro']))}, {who[c['culprit']]}, {place}, "
                   f"clues_{i}, {len(c['clues'])}, statements_{i}, {len(c['statements'])}, "
                   f"{cstr(c['confession'])}, {cstr(unquote_shio(c['wrapup']))} }},\n")
    out.append(f"}};\nconst int case_count = {len(db)};\n")
    open(os.path.join(ROOT, "src", "gen", "cases_data.c"), "w").write("".join(out))
    print(f"bake: {len(db)} cases -> src/gen/cases_data.c")


if __name__ == "__main__":
    if sys.argv[1] == "gen":
        gen(int(sys.argv[2]))
    else:
        bake()
