#!/usr/bin/env python3
"""Write the dating-sim scenes (build 06) from the character cards: date_gen.py [SPK[/BEAT],...]

For every beat in tools/date_spec.py the writer (DeepSeek, pinned) gets the character's full source card, her
in-game role, the story so far, the beat, the card's triggers and the cue vocabulary, and returns one scene as JSON.
Every scene is validated (ASCII, lengths, cue names, the character's own gestures, valid triggers, each choice
offering at least one progress and one regression trigger); a failing scene goes back with the errors, up to three
tries. Output: content/date/<SPK>__<beat>.json (resumable: existing scenes are kept)."""
import concurrent.futures as cf, json, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ai import chat, spent  # noqa: E402
from distill_spec import CARDS  # noqa: E402
import date_spec as D  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "content", "date")
MODEL = "deepseek/deepseek-v4.1-flash"
NAMES = {"NIA": "Nia", "MAKO": "Mako", "SHIO": "Shio"}
CHOICES = {"intro": 2, "milestone": 2, "hangout": 1, "ending": 1}
LENGTH = {"intro": "10 to 16", "milestone": "12 to 18", "hangout": "8 to 12", "ending": "12 to 18"}
# the other builds pass the card as "not a romance"; here its stages, milestones and triggers are the route
CARD_INTRO = ("Source card (the authority for her personality, her voice and how the relationship develops: its "
              "stages, milestones and triggers are this route):")


def ascii_clean(s):
    s = str(s).replace("’", "'").replace("‘", "'").replace("“", '"').replace("”", '"')
    s = s.replace("…", "...").replace("—", " - ").replace("–", "-")
    return re.sub(r"\s+", " ", s).strip()


def check_line(ln, spk, errs, where):
    if isinstance(ln, dict) and isinstance(ln.get("line"), dict):
        ln = ln["line"]                               # reactions written like steps: {"line": {...}}
    if not isinstance(ln, dict):
        errs.append(f"{where}: not an object"); return None
    who = str(ln.get("who", "")).strip().lower()
    who = {"she": "her", NAMES[spk].lower(): "her", "player": "you", "me": "you", "i": "you",
           "narration": "narrator", "black": "fade", "fade to black": "fade", "fade_to_black": "fade",
           "fadeout": "fade"}.get(who, who)            # the writer often names her instead of writing "her"
    if who not in ("her", "you", "narrator", "fade"):
        errs.append(f"{where}: who must be her, you, narrator or fade"); return None
    text = ascii_clean(ln.get("text", ""))
    if not text or len(text) > (40 if who == "fade" else 150):
        errs.append(f"{where}: text missing or over {40 if who == 'fade' else 150} characters ({len(text)})")
    if not all(32 <= ord(ch) < 127 for ch in text):
        errs.append(f"{where}: non-ASCII characters")
    out = {"who": who, "text": text}
    if who == "her":
        def pick(key, allowed, default):              # a near miss ("curl_slow", "down") maps to the closest cue
            v = str(ln.get(key, default)).lower()
            if v in allowed:
                return v
            near = [a for a in allowed if a in v]
            return near[0] if near else {"down": "droop", "low": "droop", "open": "mid", "near": "close",
                                         "closer": "close", "back": "far"}.get(v, default)
        out["face"] = pick("face", D.FACES, "neutral")
        out["ears"] = pick("ears", D.EARS, "up")
        out["tail"] = pick("tail", D.TAILS, "sway")
        out["pose"] = pick("pose", D.POSES, "mid")
        g = str(ln.get("gesture", "none")).lower()
        out["gesture"] = g if g in D.GESTURES[spk] else "none"
        out["blush"] = bool(ln.get("blush", False))
        out["look_away"] = bool(ln.get("look_away", False))
    return out


def validate(scene, spk, kind):
    errs = []
    if not isinstance(scene, dict):
        return None, ["reply is not a JSON object"]
    loc = scene.get("location")
    if loc not in D.LOCATIONS:
        errs.append(f"location must be one of {D.LOCATIONS}")
    steps, n_lines, n_choices = [], 0, 0
    trig = D.TRIGGERS[spk]
    prim = D.PRIMARY[spk]
    for i, st in enumerate(scene.get("steps") or []):
        if isinstance(st, dict) and "line" in st:
            ln = check_line(st["line"], spk, errs, f"step {i}")
            if ln:
                steps.append({"line": ln}); n_lines += 1
        elif isinstance(st, dict) and "choice" in st:
            ch = st["choice"] or {}
            opts = []
            for j, op in enumerate(ch.get("options") or []):
                t = op.get("trigger")
                if t not in trig:
                    errs.append(f"step {i} option {j}: trigger '{t}' is not one of {sorted(trig)}")
                    continue
                text = ascii_clean(op.get("text", ""))
                if not text or len(text) > 70:
                    errs.append(f"step {i} option {j}: text missing or over 70 characters")
                reacts = [check_line(r, spk, errs, f"step {i} option {j} reaction {k}")
                          for k, r in enumerate(op.get("reaction") or [])]
                reacts = [r for r in reacts if r]
                if not 1 <= len(reacts) <= 3:
                    errs.append(f"step {i} option {j}: 1 to 3 reaction lines")
                opts.append({"text": text, "trigger": t, "reaction": reacts})
            if not 3 <= len(opts) <= 4:
                errs.append(f"step {i}: a choice needs 3 or 4 options")
            gains = [trig[o["trigger"]][1].get(prim, 0) for o in opts]
            if opts and (max(gains) <= 0 or min(gains) >= 0):
                errs.append(f"step {i}: each choice needs at least one option that raises {prim} and one that lowers it")
            steps.append({"choice": {"prompt": ascii_clean(ch.get("prompt", ""))[:80], "options": opts}})
            n_choices += 1
    if steps and "choice" not in steps[-1]:
        errs.append("the scene must end with the final choice (no steps after the last choice)")
    if n_choices != CHOICES[kind]:
        errs.append(f"the scene needs exactly {CHOICES[kind]} choice(s), found {n_choices}")
    if n_lines < 6:
        errs.append("too few lines")
    fades = sum(1 for st in steps if "line" in st and st["line"]["who"] == "fade")
    fades += sum(1 for st in steps if "choice" in st for o in st["choice"]["options"] for r in o["reaction"]
                 if r["who"] == "fade")
    if fades > 1:
        errs.append("at most one fade to black per scene")
    time = scene.get("time") if scene.get("time") in ("evening", "late") else "evening"
    return {"location": loc if loc in D.LOCATIONS else "doorway", "time": time, "steps": steps}, errs


def story_so_far(spk, beat_id):
    done = []
    for bid, kind, thr, desc, _ in D.BEATS[spk]:
        if bid == beat_id:
            break
        if kind in ("intro", "milestone"):
            done.append(f"- {desc}")
    return "\n".join(done) or "- (nothing yet: this is the first evening)"


def write_scene(spk, beat):
    bid, kind, thr, desc, outfit = beat
    path = os.path.join(OUT, f"{spk}__{bid}.json")
    if os.path.exists(path):
        return spk, bid, "kept", 0
    name = NAMES[spk]
    trig = "\n".join(f"  {k}: {v[0]}" for k, v in D.TRIGGERS[spk].items())
    system = ("You write scenes for a cozy pixel-art dating sim about cat demi-humans living in one apartment "
              "building. " + D.WRITER_RULES)
    user = (f"Character: {name}.\n{CARD_INTRO}\n{CARDS[spk]}\n\nIn this game she is {D.ROLE[spk]}\n\n"
            f"The story so far (earlier evenings with her):\n{story_so_far(spk, bid)}\n\n"
            f"This scene ({kind}): {desc}\n\n"
            f"Write {LENGTH[kind]} steps in all, including exactly {CHOICES[kind]} choice(s) for the player. "
            f"Her gestures available: {D.GESTURES[spk]}.\n"
            f"Triggers (each option names one; they are the card's progress and regression triggers):\n{trig}\n\n"
            f"Cue values: face {D.FACES}; ears {D.EARS}; tail {D.TAILS}; pose {D.POSES}; "
            f"location {D.LOCATIONS}.\n\nJSON shape:\n{D.SCHEMA_HINT}")
    errs, scene = [], None
    for attempt in range(3):
        msg = user if not errs else user + "\n\nYour previous reply had these problems; fix them all:\n- " + \
            "\n- ".join(errs[:12])
        try:
            raw = chat(system, msg, json_mode=True, temperature=0.9, max_tokens=6000, model=MODEL,
                       tag=f"date/{spk}/{bid}")
            scene, errs = validate(json.loads(raw), spk, kind)
        except (Exception, SystemExit) as e:
            scene, errs = None, [f"could not parse the reply: {str(e)[:100]}"]
        if scene and not errs:
            break
    if not scene or errs:
        return spk, bid, "FAILED: " + "; ".join(errs[:3]), 0
    scene.update({"spk": spk, "beat": bid, "kind": kind, "threshold": thr, "outfit_unlock": outfit, "desc": desc})
    os.makedirs(OUT, exist_ok=True)
    json.dump(scene, open(path, "w"), indent=1)
    return spk, bid, "written", sum(1 for s in scene["steps"] if "line" in s)


def main():
    only = set(sys.argv[1].split(",")) if len(sys.argv) > 1 else None
    todo = [(spk, b) for spk in D.BEATS for b in D.BEATS[spk]
            if not only or spk in only or f"{spk}/{b[0]}" in only]
    print(f"date_gen: {len(todo)} scenes; spend so far ${spent():.4f}", flush=True)
    with cf.ThreadPoolExecutor(max_workers=6) as pool:
        for spk, bid, status, n in pool.map(lambda a: write_scene(*a), todo):
            print(f"  {spk:5s} {bid:12s} {status} {n or ''}", flush=True)
    print(f"date_gen: done; spend so far ${spent():.4f}")


if __name__ == "__main__":
    main()
