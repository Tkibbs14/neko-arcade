#!/usr/bin/env python3
"""Bake the dating-sim scenes (content/date/*.json, written by tools/date_gen.py) into src/gen/date_data.c/.h.

Steps are one flat array: LINE (who, cues, text), CHOICE (prompt, number of options), then per option an OPTION
(text, trigger) followed by its reaction LINEs and an OPT_END, and END closing the scene. Triggers become the card's
stat changes (tools/date_spec.py), so the numbers on the device are the card's own.

The writer puts cues (face, ears, tail, pose, gesture) on her lines. Narration describes tells too ("her tail gives
one sharp flick, then goes completely still"), so every narrated line is read here for the tells it names and
carries them as cues, and the portrait acts out what the text says: the first clause's tells at once, the later
ones (after "then", a comma, "before") as a CUE step a moment later. A field the narration does not mention is KEEP.
Knocks, thunder and lightning in the narration become sounds and flashes. Choice options are shuffled with a fixed
seed per choice, so the kind answer is not always in the same place."""
import json, os, random, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import date_spec as D  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NL, BS = chr(10), chr(92)
WHO = {"NIA": 0, "MAKO": 1, "SHIO": 2}
KINDS = {"intro": 0, "milestone": 1, "hangout": 2, "ending": 3}
GEST = {"none": 0, "glasses": 1, "wrist": 2, "chain": 3, "fringe": 4}
ENDINGS = {"end_best": 0, "end_friends": 1, "end_drift": 2}
KEEP = 255
F_BLUSH, F_AWAY, F_EAR_FLICK, F_FLASH, F_THUNDER, F_LOOK = 1, 2, 4, 8, 16, 32
W_RAIN, W_STORM, W_DARK = 1, 2, 4
CUE_DELAY = 30                                        # frames between a narrated line's first and later tells


def cstr(s):
    return '"' + s.replace(BS, BS + BS).replace('"', BS + '"') + '"'


def beat_index(bid):
    if bid in ENDINGS:
        return ENDINGS[bid]
    if bid == "intro":
        return 0
    return int(bid[1:])                               # m1..m6, h1..h6


# ---------------------------------------------------------------- reading tells out of narration
# (phrase list, value) in priority order; the first match in a clause wins for that field. Phrases match at the
# start of a word ("low" is not in "slow"; "curl" matches "curls" and "curled").
TAIL = [
    (["goes still", "go still", "gone still", "goes completely still", "gone completely still", "is completely still",
      "is still", "stock-still", "freezes", "stops moving", "stops", "stills", "motionless"], "still"),
    (["around her ankle", "around her leg", "around her knee", "around her", "wraps", "coils"], "wrap"),
    (["tuck"], "tuck"),
    (["puff", "fluff", "bristle", "bottle", "frizz", "doubles"], "puff"),
    (["curl", "hooks", "question mark"], "curl"),
    (["wag", "swish", "thump", "sweeps", "swings"], "wag"),
    (["droop", "drop", "sinks", "lowers", "sags", "drags", "falls", "low"], "low"),
    (["flick", "twitch", "lash", "jerk", "snaps"], "flick"),
    (["sway", "relax", "release", "loosen", "settle", "resume", "uncurl", "unwind", "lifts", "rises", "raises",
      "smooth"], "sway"),
    (["still"], "still"),
]
EARS = [
    (["back to the", "back to her"], "up"),
    (["back to you", "back toward you", "toward you", "towards you", "to you", "at you", "on you", "tracking you",
      "tracks you", "track you", "track your face", "tracking your face", "following you", "forward", "focused"], "perk"),
    (["rotate back", "rotates back", "rotate backward", "rotates backward", "swivel back", "swivels back",
      "turn back", "turns back", "angle back", "angles back"], "back"),
    (["flat", "pin", "press", "plaster"], "flat"),
    (["swivel", "rotate", "turn toward", "turns toward", "track", "follow", "angle"], "swivel"),
    (["perk", "prick", "snap up", "snaps up", "shoot up", "spring up", "pop up", "lift", "rise", "raise",
      "straighten", "up"], "perk"),
    (["back", "away"], "back"),
    (["droop", "lower", "sag", "drop", "sink", "fold", "wilt", "down"], "droop"),
    (["curl"], "curl"),
    (["still", "freeze", "motionless"], "still"),
    (["relax", "settle", "ease", "loosen", "return"], "up"),
]
EAR_TWITCH = ["twitch", "flick", "flicker", "jerk", "quiver"]
FREEZE = ["goes still", "goes very still", "goes completely still", "go still", "freezes", "holds still",
          "is very still", "stays very still", "sits very still", "sits completely still"]
FACE = [
    (["smirk"], "smug"),
    (["smile", "grin", "laugh", "giggle", "beam"], "happy"),
    (["frown", "scowl", "glare", "narrow"], "annoyed"),
    (["flinch", "startle", "jumps", "blinks at"], "surprised"),
    (["sigh", "yawn", "rubs her eyes"], "tired"),
]
BLUSH = ["blush", "flush", "pink", "color rises", "colour rises", "goes red", "turns red", "cheeks"]
AWAY = ["looks away", "glances away", "look away", "eyes drop", "avoids", "gaze slides", "looks down", "glances down",
        "back to her screen", "back to the screen", "back to her laptop", "back to the laptop", "studies the",
        "stares at the", "stares at her", "looks at the floor", "turns back to the", "looks at the",
        "instead of you", "gaze drops", "drops to the floor"]
LOOK = ["looks up", "glances up", "looks at you", "meets your eyes", "meets your gaze", "holds your gaze",
        "glances at you", "looks right at you"]
POSE = [
    (["leans in", "lean in", "leans closer", "leans toward", "leans against", "leans on", "leans her",
      "rests her head", "head on your shoulder", "forehead on your shoulder"], "lean"),
    (["back inside", "steps back in"], "mid"),
    (["steps back", "step back", "backs away", "retreats", "pulls back", "into the hallway", "to the hallway",
      "toward the door", "towards the door", "toward her own door", "half-step back"], "far"),
    (["steps closer", "step closer", "moves closer", "shifts closer", "sits closer", "scoots", "edges closer",
      "comes closer", "an inch closer", "a little closer", "closer than before", "shoulder to shoulder",
      "sits next to", "sits beside", "half a pace", "shoulder almost touching", "shoulder touching"], "close"),
    (["turns away", "turns her back", "toward the window", "towards the window", "puts the room between",
      "walks to the", "faces the"], "away"),
    (["steps in", "steps inside", "comes in", "walks in", "sits down", "sits on", "takes a seat", "settles onto"], "mid"),
]
GESTURE = {  # her hands doing it, not the thing being there ("sleeves past her wrists" is not a gesture)
    "NIA": [(["pushes her fringe", "pushes the fringe", "fringe back", "brushes her fringe", "fixing her fringe",
              "tugs her fringe", "hair out of her", "hair from her", "pushes her bangs", "bangs back"], "fringe")],
    "MAKO": [(["touches her chain", "touches the chain", "fingers the chain", "fingers her chain", "tugs the chain",
               "tugs her chain", "twists the chain", "twists her chain", "hand to her chain", "hand goes to her chain",
               "chain necklace taps"], "chain")],
    "SHIO": [(["pushes her glasses", "pushes the glasses", "adjusts her glasses", "adjusts the glasses",
               "takes off her glasses", "cleans her glasses", "straightens her glasses", "touches her glasses",
               "glasses up", "fixes her glasses", "nudges her glasses"], "glasses"),
             (["touches her wrist", "taps her wrist", "rubs her wrist", "presses her wrist", "holds her wrist",
               "circles her wrist", "against her wrist", "turns of the wrist", "her own wrist", "to her wrist"],
              "wrist")]}
NUMS = {"one": 1, "two": 2, "three": 3, "four": 4, "five": 5, "a": 1, "single": 1}
SUBJECTS = ("she", "her", "nia", "mako", "shio", "you", "your", "the", "it", "its", "there", "this", "that",
            "minutes", "something")
LINKS = ("and", "but", "so", "yet", "even", "as")


def norm(s):
    return " " + " ".join(re.findall("[a-z'-]+", s.lower())) + " "


def has(clause, phrase):
    if phrase.endswith(" you"):                       # "toward you" is not "toward your door"
        return (" " + phrase + " ") in clause
    return (" " + phrase) in clause                   # clause is norm()ed: phrase at a word start


def first_match(table, clause):
    for phrases, value in table:
        if any(has(clause, p) for p in phrases):
            return value
    return None


def negated(clause):
    words = clause.split()
    return any(w in ("not", "never", "without") or w.endswith("n't") for w in words) or has(clause, "no longer")


def clauses(text):
    """Split narration into clauses in the order things happen; 'X before Y' is X then Y."""
    t = " " + text.lower() + " "
    for sep in (" and then ", ", then ", " then ", " before ", " but ", " - ", ";", ":", ",", ".", "!", "?"):
        t = t.replace(sep, "|")
    return [norm(c) for c in t.split("|") if c.strip()]


def narration_cues(text, spk):
    """Stages of cues [{field: value}], at most two (now, a moment later), plus flags and the knock pattern."""
    stages, flags, subject = [], 0, None
    low = norm(text)
    if has(low, "lightning") or has(low, "whitens"):
        flags |= F_FLASH
    if has(low, "thunder"):
        flags |= F_THUNDER
    for c in clauses(text):
        words = [w for w in c.split() if w not in LINKS] or [""]
        if has(c, "tail") or has(c, "tip"):
            subject = "tail"
        elif has(c, "ear"):
            subject = "ears"
        elif has(c, "gaze") or has(c, "eyes"):
            subject = None
        elif words[0] in SUBJECTS and not (subject and (has(c, "it") or has(c, "them"))):
            subject = None                            # "then settles" and "she stills it" carry; "she turns" does not
        if (has(low, "yui") and not has(c, "mako")) or negated(c):   # someone else, or a thing she does not do
            continue
        cue = {}
        if subject == "tail":
            v = first_match(TAIL, c)
            if v:
                cue["tail"] = v
        elif subject == "ears":
            rest = norm(c.split(" ear", 1)[-1]) if has(c, "ear") else c
            v = first_match(EARS, rest)
            if v:
                cue["ears"] = v
            elif any(has(rest, p) for p in EAR_TWITCH):
                cue["ear_flick"] = True
        elif any(has(c, p) for p in FREEZE):          # "she goes very still": all of her, tail and ears
            cue["tail"] = "still"
            cue["ears"] = "still"
        v = first_match(FACE, c)
        if v:
            cue["face"] = v
        if any(has(c, p) for p in BLUSH):
            cue["blush"] = True
        if any(has(c, p) for p in AWAY):
            cue["away"] = True
        elif any(has(c, p) for p in LOOK):
            cue["look"] = True
        v = first_match(POSE, c)
        if v:
            cue["pose"] = v
        v = first_match(GESTURE[spk], c)
        if v:
            cue["gesture"] = v
        if cue:
            stages.append(cue)
    if len(stages) > 2:                               # the first tell now, everything after it a moment later
        later = {}
        for s in stages[1:]:
            later.update(s)
        stages = [stages[0], later]
    knock = 0
    words = low.split()
    if any(w in ("knock", "knocks") for w in words):  # "since she knocked" is backstory, not a sound
        sharp = 1
        for i, w in enumerate(words):
            if w in ("knock", "knocks"):
                back = [NUMS[x] for x in words[max(0, i - 3):i] if x in NUMS]
                sharp = back[-1] if back else 1
                break
        m = re.search("(one|two|three|four) (sharp|precise|quick|hard)", low)
        if m:
            sharp = NUMS[m.group(1)]
        if has(low, "fourth"):
            sharp += 1
        soft = 1 if any(has(low, w) for w in ("softer", "soft", "quieter", "gentler", "lighter")) else 0
        if soft and (has(low, "the third softer") or has(low, "the last softer") or sharp == 1):
            sharp -= 1                                # "three knocks, the third softer"; "a soft knock"
        knock = min(sharp, 5) | (soft << 4)
    return stages, flags, knock


def cue_fields(cue):
    face = D.FACES.index(cue["face"]) if "face" in cue else KEEP
    ears = D.EARS.index(cue["ears"]) if "ears" in cue else KEEP
    tail = D.TAILS.index(cue["tail"]) if "tail" in cue else KEEP
    pose = D.POSES.index(cue["pose"]) if "pose" in cue else KEEP
    gest = GEST[cue["gesture"]] if "gesture" in cue else KEEP
    flags = ((F_BLUSH if cue.get("blush") else 0) | (F_AWAY if cue.get("away") else 0)
             | (F_EAR_FLICK if cue.get("ear_flick") else 0) | (F_LOOK if cue.get("look") else 0))
    return face, ears, tail, pose, gest, flags


# ---------------------------------------------------------------- baking
steps, scenes, missing = [], [], []
stats = {"narrated": 0, "with_tells": 0, "two_stage": 0, "knocks": 0, "flashes": 0}
samples = []


def step(kind, who=0, face=0, ears=0, tail=0, pose=0, gest=0, flags=0, arg=0, text=None):
    steps.append(f"    {{ {kind}, {who}, {face}, {ears}, {tail}, {pose}, {gest}, {flags}, {arg}, "
                 f"{cstr(text) if text is not None else 0} }},{NL}")


def line(ln, spk):
    who = {"her": 0, "you": 1, "narrator": 2, "fade": 3}[ln["who"]]
    if who == 1:
        step("DS_LINE", 1, text=ln["text"])
        return
    if who == 3:                                      # a fade to black: the text shows over black
        step("DS_LINE", 3, KEEP, KEEP, KEEP, KEEP, KEEP, 0, 0, ln["text"])
        return
    if who == 0:
        flags = (F_BLUSH if ln.get("blush") else 0) | (F_AWAY if ln.get("look_away") else 0)
        step("DS_LINE", 0, D.FACES.index(ln["face"]), D.EARS.index(ln["ears"]), D.TAILS.index(ln["tail"]),
             D.POSES.index(ln["pose"]), GEST[ln["gesture"]], flags, 0, ln["text"])
        return
    stages, env, knock = narration_cues(ln["text"], spk)
    stats["narrated"] += 1
    stats["with_tells"] += bool(stages)
    stats["two_stage"] += len(stages) == 2
    stats["knocks"] += bool(knock)
    stats["flashes"] += bool(env)
    if stages and len(samples) < 400:
        samples.append((spk, ln["text"], stages))
    first = cue_fields(stages[0]) if stages else (KEEP,) * 5 + (0,)
    step("DS_LINE", 2, *first[:5], first[5] | env, knock, ln["text"])
    if len(stages) == 2:
        step("DS_CUE", 2, *cue_fields(stages[1]), CUE_DELAY)


for spk in WHO:
    trig_ids = list(D.TRIGGERS[spk])
    for bid, kind, thr, desc, outfit in D.BEATS[spk]:
        path = os.path.join(ROOT, "content", "date", f"{spk}__{bid}.json")
        if not os.path.exists(path):
            missing.append(f"{spk}/{bid}")
            continue
        sc = json.load(open(path))
        step0 = len(steps)
        narr = " ".join(st["line"]["text"] for st in sc["steps"] if "line" in st and st["line"]["who"] == "narrator")
        weather = 0
        if any(w in (desc + " " + narr).lower() for w in ("rain", "storm", "thunder", "drizzle", "downpour")):
            weather |= W_RAIN
        if any(w in (desc + " " + narr).lower() for w in ("thunder", "lightning", "storm")):
            weather |= W_STORM
        if any(w in desc.lower() for w in ("power cut", "goes dark", "blackout")):
            weather |= W_DARK
        for ci, st in enumerate(sc["steps"]):
            if "line" in st:
                line(st["line"], spk)
                continue
            ch = st["choice"]
            opts = list(ch["options"])
            random.Random(f"{spk}/{bid}/{ci}").shuffle(opts)
            step("DS_CHOICE", arg=len(opts), text=ch["prompt"])
            for op in opts:
                step("DS_OPTION", arg=trig_ids.index(op["trigger"]), text=op["text"])
                for r in op["reaction"]:
                    line(r, spk)
                step("DS_OPT_END")
        step("DS_END")
        scenes.append(f"    {{ {WHO[spk]}, {KINDS[kind]}, {beat_index(bid)}, {thr}, "
                      f"{D.LOCATIONS.index(sc['location'])}, {1 if sc['time'] == 'late' else 0}, {outfit}, "
                      f"{weather}, {step0}, {cstr(desc)} }},{NL}")

if missing:
    sys.exit("mkdate: scenes missing (run tools/date_gen.py): " + ", ".join(missing))

eff, tnames, snames, sstart, prim = [], [], [], [], []
for spk in WHO:
    stat_ids = list(D.STATS[spk])
    rows = []
    for tid, (_, fx) in D.TRIGGERS[spk].items():
        d = [fx.get(s, 0) for s in stat_ids] + [0] * (4 - len(stat_ids))
        rows.append("{ { " + ", ".join(map(str, d)) + " } }")
    rows += ["{ { 0, 0, 0, 0 } }"] * (16 - len(rows))
    eff.append("    { " + ", ".join(rows) + " }," + NL)
    tnames.append("    { " + ", ".join(cstr(t) for t in D.TRIGGERS[spk]) + " }," + NL)
    snames.append("    { " + ", ".join(cstr(s) for s in stat_ids) + " }," + NL)
    sstart.append("    { " + ", ".join(str(D.STATS[spk][s]) for s in stat_ids) + " }," + NL)
    prim.append(str(stat_ids.index(D.PRIMARY[spk])))

c = ["/* Generated by tools/mkdate.py - the dating sim's scenes (build 06), written from the character cards. */" + NL,
     '#include "date_data.h"' + NL + NL,
     f"const DStep date_steps[{len(steps)}] = {{{NL}", *steps, "};" + NL,
     f"const DScene date_scenes[] = {{{NL}", *scenes, "};" + NL,
     f"const int date_scene_count = {len(scenes)};{NL}",
     "const DEffect date_effects[3][16] = {" + NL, *eff, "};" + NL,
     "const char *const date_trigger_names[3][16] = {" + NL, *tnames, "};" + NL,
     "const char *const date_stat_names[3][4] = {" + NL, *snames, "};" + NL,
     "const u8 date_stat_start[3][4] = {" + NL, *sstart, "};" + NL,
     f"const u8 date_primary[3] = {{ {', '.join(prim)} }};{NL}",
     f"const int date_evenings = {D.EVENINGS};{NL}"]
h = NL.join([
    "/* Generated by tools/mkdate.py - the dating sim's scenes (build 06). */",
    "#pragma once",
    '#include "../engine.h"',
    "",
    "enum { DS_LINE, DS_CHOICE, DS_OPTION, DS_OPT_END, DS_END, DS_CUE };",
    "enum { DK_INTRO, DK_MILESTONE, DK_HANGOUT, DK_ENDING };",
    "enum { DL_DOORWAY, DL_KITCHEN, DL_COUCH, DL_HALLWAY, DL_STAIRWELL, DL_STORE, DL_HER_ROOM, DL_COUNT };",
    f"#define DC_KEEP {KEEP}",
    f"enum {{ DF_BLUSH = {F_BLUSH}, DF_AWAY = {F_AWAY}, DF_EAR_FLICK = {F_EAR_FLICK}, DF_FLASH = {F_FLASH}, "
    f"DF_THUNDER = {F_THUNDER}, DF_LOOK = {F_LOOK} }};",
    f"enum {{ DW_RAIN = {W_RAIN}, DW_STORM = {W_STORM}, DW_DARK = {W_DARK} }};",
    "/* LINE: who 0 her, 1 you, 2 narrator, 3 fade; cues face/ears/tail/pose/gesture as portrait.h (narration: DC_KEEP where it",
    " * names no tell); for narration arg = knocks (low nibble sharp, high nibble soft). CUE: the narrated line's later",
    " * tells, arg frames after the line starts. CHOICE: arg = options that follow. OPTION: arg = trigger (date_effects). */",
    "typedef struct { u8 kind, who, face, ears, tail, pose, gesture, flags; i16 arg; const char *text; } DStep;",
    "typedef struct { u8 who, kind, beat, threshold, loc, late, outfit, weather; u16 step0; const char *desc; } DScene;",
    "typedef struct { i8 d[4]; } DEffect;",
    "",
    "extern const DStep date_steps[];",
    "extern const DScene date_scenes[];",
    "extern const int date_scene_count;",
    "extern const DEffect date_effects[3][16];",
    "extern const char *const date_trigger_names[3][16];",
    "extern const char *const date_stat_names[3][4];",
    "extern const u8 date_stat_start[3][4];",
    "extern const u8 date_primary[3];",
    "extern const int date_evenings;",
    ""])
open(os.path.join(ROOT, "src", "gen", "date_data.c"), "w").write("".join(c))
open(os.path.join(ROOT, "src", "gen", "date_data.h"), "w").write(h)
print(f"mkdate: {len(scenes)} scenes, {len(steps)} steps -> src/gen/date_data.c")
print("mkdate: narration", stats)
if "--samples" in sys.argv:
    for spk, text, st in samples[:int(sys.argv[sys.argv.index("--samples") + 1])]:
        print(f"  {spk:4s} {text[:90]:90s} {st}")
