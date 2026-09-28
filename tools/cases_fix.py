#!/usr/bin/env python3
"""Human review pass over the AI-written cases (2026-09-27): make each contradiction prove the culprit's lie
and remove self-inconsistent testimony, then re-run the logic checker on every case."""
import json, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cases_gen import DB, check  # noqa: E402

db = json.load(open(DB))
by_title = {c["title"]: c for c in db}


def stmt(case, who, starts):
    for s in case["statements"]:
        if s["who"] == who and s["text"].startswith(starts):
            return s
    raise KeyError(f"{case['title']}: {who} {starts}")


def clue(case, cid):
    return next(c for c in case["clues"] if c["id"] == cid)


c = by_title["The Vanishing Muffins"]          # crumbs only proved someone used the scale
c["intro"] = "A batch of Mocha's muffins vanished from the cafe kitchen overnight. Let's find the culprit."
clue(c, "crumbs")["name"] = "Scale"
clue(c, "crumbs")["desc"] = "Chocolate crumbs on the scale, Mako's pink measuring cup still on top."

c = by_title["The Empty Cream Pitcher"]        # the apron is the clue that exposes the lie
stmt(c, "MOCHA", "I closed the cafe")["contradicted_by"] = None
stmt(c, "MOCHA", "I always wash my apron")["contradicted_by"] = "mocha_apron"
clue(c, "mocha_apron")["desc"] = "Mocha's apron, folded on a chair, with a fresh cream stain on the pocket."

c = by_title["The Missing Ramen Packet"]       # Mocha could not be up at 5 and see Nia at 2
stmt(c, "MOCHA", "I was up at 5 am")["text"] = "I got up for water around 2 am and saw Nia's door open."
stmt(c, "MOCHA", "Nia looked guilty")["text"] = "The hallway smelled like spicy ramen for an hour after."

c = by_title["The Great Egg Heist"]            # Mako cannot steal her own eggs
c["intro"] = "Shio briefs: 'Mocha's dozen eggs for the morning rush vanished from the fridge. Let's crack this.'"
clue(c, "egg_carton")["desc"] = "Empty egg carton in the trash, labeled MOCHA - CAFE, bought yesterday."
stmt(c, "MAKO", "I bought eggs yesterday")["text"] = "I use my own eggs. Mine are still in the fridge, you can count."

bad = 0
for case in db:
    problems = check(case)
    print(f"{case['title']:28s} {'ok' if not problems else problems}")
    bad += bool(problems)
if bad:
    raise SystemExit("fix failed the checker")
json.dump(db, open(DB, "w"), indent=1)
