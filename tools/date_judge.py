#!/usr/bin/env python3
"""Panel check of the dating-sim scenes (build 06): date_judge.py TAG [BEAT,BEAT,...]

Every scene goes to the three pinned judges (tools/nlm_quality.JUDGES, three model families) with the character's
source card. Each of her lines and each narrated line is rated for sense (0-2), voice against the card (0-2, her
lines) and heat (2 all-ages, 1 suggestive, which the mode allows, 0 explicit, which it does not). Reasoning is kept
per call. Reported per judge and as
the median of three; lines flagged by two or more judges are listed for fixing.
Output: build/quality/TAG/{raw.jsonl, report.md}."""
import concurrent.futures as cf, glob, json, os, statistics, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from nlm_quality import JUDGES, ask  # noqa: E402
from distill_spec import CARDS  # noqa: E402
from ai import spent  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NAMES = {"NIA": "Nia", "MAKO": "Mako", "SHIO": "Shio"}
NL = chr(10)
SYSTEM = """You review scenes from a cozy pixel-art dating sim about adult cat demi-humans in one apartment building.
Flirting, innuendo, suggestive dialogue and closeness (a kiss, a hand, leaning in) are allowed; a moment that would go
further cuts away with a "fade" line (text over a black screen). Nothing explicit may be on screen: no sexual acts, no
nudity, no sexual description of bodies. You get the character's source card (the authority for her personality and
voice) and one scene as numbered lines.
Rate every numbered line spoken by her ("her"), the narrator ("narrator") or a "fade" line:
- sense: 2 = clear and makes sense in the scene; 1 = understandable but awkward or vague; 0 = nonsense or contradicts
  the scene.
- voice (her lines only; use null for narration): 2 = sounds like the card's character; 1 = neutral, anyone could say
  it; 0 = contradicts the card.
- pg: 2 = fine for all ages; 1 = suggestive (allowed here); 0 = explicit (not allowed: sexual acts, nudity or sexual
  description of bodies on screen).
Reply with JSON only: {"ratings": [{"n": 1, "sense": 2, "voice": 2, "pg": 2, "why": "under 12 words"}]} with one
entry per rated line, in order."""


def script(sc):
    """Numbered lines: (n, who, text); choices are shown with their options and each option's reaction."""
    rows, n = [], 0

    def add(who, text, indent=""):
        nonlocal n
        n += 1
        rows.append((n, who, text, indent))

    for st in sc["steps"]:
        if "line" in st:
            add(st["line"]["who"], st["line"]["text"])
        else:
            ch = st["choice"]
            rows.append((None, "choice", ch["prompt"], ""))
            for k, op in enumerate(ch["options"]):
                rows.append((None, "option", f"{k + 1}. {op['text']}", "  "))
                for r in op["reaction"]:
                    add(r["who"], r["text"], "    ")
    return rows


def render(rows):
    out = []
    for n, who, text, ind in rows:
        if n is None:
            out.append(f"{ind}[{who}] {text}")
        else:
            out.append(f"{ind}{n}. {who}: {text}")
    return NL.join(out)


def job(path, model):
    sc = json.load(open(path))
    spk = sc["spk"]
    rows = script(sc)
    user = (f"Character: {NAMES[spk]}.{NL}Source card:{NL}{CARDS[spk]}{NL}{NL}This scene ({sc['kind']}, "
            f"{sc['location']}): {sc['desc']}{NL}{NL}{render(rows)}")
    try:
        content, reasoning, provider, served = ask(model, user, SYSTEM, tag="date-judge", effort="low")
        ratings = json.loads(content).get("ratings", [])
    except Exception as e:  # noqa: BLE001 - a failed call is reported, not retried silently
        return {"scene": os.path.basename(path), "model": model, "error": str(e)[:200]}
    texts = {n: (who, text) for n, who, text, _ in rows if n}
    return {"scene": os.path.basename(path), "model": model, "provider": provider, "served": served,
            "ratings": ratings, "texts": texts, "reasoning": reasoning[:4000]}


def main():
    tag = sys.argv[1] if len(sys.argv) > 1 else "date1"
    out = os.path.join(ROOT, "build", "quality", tag)
    os.makedirs(out, exist_ok=True)
    scenes = sorted(glob.glob(os.path.join(ROOT, "content", "date", "*.json")))
    if len(sys.argv) > 2:                             # only these beats, e.g. m4,m5,m6,end_best
        beats = set(sys.argv[2].split(","))
        scenes = [p for p in scenes if os.path.basename(p).split("__")[1][:-5] in beats]
    jobs = [(p, m) for p in scenes for m in JUDGES]
    print(f"date_judge: {len(scenes)} scenes x {len(JUDGES)} judges; spend so far ${spent():.4f}", flush=True)
    results = []
    with cf.ThreadPoolExecutor(max_workers=6) as pool:
        for r in pool.map(lambda a: job(*a), jobs):
            results.append(r)
            if "error" in r:
                print(f"  {r['scene']} {r['model']}: ERROR {r['error']}", flush=True)
    with open(os.path.join(out, "raw.jsonl"), "w") as f:
        for r in results:
            f.write(json.dumps(r) + NL)
    # per line: judge -> rating
    lines = {}
    for r in results:
        for x in r.get("ratings", []):
            n = x.get("n")
            if n not in r["texts"]:
                continue
            key = (r["scene"], n)
            who, text = r["texts"][n]
            lines.setdefault(key, {"who": who, "text": text, "j": {}})["j"][r["model"]] = x
    rep = [f"# Thin Walls scene check ({tag})", "",
           f"{len(scenes)} scenes, {len(lines)} rated lines (hers and narration), judged by {', '.join(JUDGES)} with the "
           "source cards. Errors: " + str(sum('error' in r for r in results)) + " calls.", ""]
    rep += ["| judge | sense 2 | sense 0 | voice 2 (her lines) | voice 0 | all-ages | explicit |",
            "|---|---|---|---|---|---|---|"]

    def pct(a, b):
        return f"{100 * a / b:.1f}%" if b else "-"

    for m in JUDGES + ["median of 3"]:
        s2 = s0 = v2 = v0 = vn = p2 = p0 = n = 0
        for ln in lines.values():
            js = ln["j"]
            if m == "median of 3":
                if len(js) < 2:
                    continue
                def med(k):
                    vals = [v[k] for v in js.values() if isinstance(v.get(k), int)]
                    return int(statistics.median_low(vals)) if vals else None
                x = {"sense": med("sense"), "voice": med("voice"), "pg": med("pg")}
            elif m in js:
                x = js[m]
            else:
                continue
            n += 1
            s2 += x.get("sense") == 2
            s0 += x.get("sense") == 0
            p2 += x.get("pg") == 2
            p0 += x.get("pg") == 0
            if ln["who"] == "her" and isinstance(x.get("voice"), int):
                vn += 1
                v2 += x["voice"] == 2
                v0 += x["voice"] == 0
        rep.append(f"| {m} | {pct(s2, n)} | {pct(s0, n)} | {pct(v2, vn)} | {pct(v0, vn)} | {pct(p2, n)} | {pct(p0, n)} |")
    rep += ["", "## Lines flagged by two or more judges", ""]
    for (scene, n), ln in sorted(lines.items()):
        flags = []
        for k, label in (("pg", "explicit"), ("sense", "nonsense"), ("voice", "off-voice")):
            bad = [m.split("/")[1] for m, x in ln["j"].items() if x.get(k) == 0]
            if len(bad) >= 2:
                flags.append(f"{label} ({', '.join(bad)})")
        if flags:
            whys = "; ".join(str(x.get("why", "")) for x in ln["j"].values())
            rep.append(f"- `{scene}` #{n} {ln['who']}: \"{ln['text']}\" - {', '.join(flags)}. Why: {whys}")
    open(os.path.join(out, "report.md"), "w").write(NL.join(rep) + NL)
    print(NL.join(rep[:12]))
    print(f"date_judge: done; spend so far ${spent():.4f}; report build/quality/{tag}/report.md")


if __name__ == "__main__":
    main()
