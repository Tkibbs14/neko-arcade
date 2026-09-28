#!/usr/bin/env python3
"""NekoLM quality check.

  nlm_quality.py build TAG        evaluation set from build/quality/student.tsv (tools/quality.sh) + teacher controls
  nlm_quality.py judge TAG        blind ratings from a pinned three-family judge panel (OpenRouter), reasoning kept
  nlm_quality.py report TAG       per-judge and median-of-three results -> build/quality/TAG/report.md

Lines are judged as the player sees them ({item} and {n} filled with real in-game values). The set mixes, blind:
student lines the on-device check passes (what players see from the model), student lines it rejects (is the
gate selective?), and the teacher's own lines for the same speaker and situation (the ceiling).
"""
import collections, concurrent.futures as cf, json, os, random, re, sys, threading, time, urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ai import _key, _log_lock, LOG, spent  # noqa: E402
from distill_spec import VOICES, SITS  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TAGS = json.load(open(os.path.join(ROOT, "content", "distill", "tags.json")))
SPK = {v: k for k, v in TAGS["speakers"].items()}
SIT = {v: k for k, v in TAGS["situations"].items()}
DESC = {(s, t): d for s, t, d in SITS}
JUDGES = ["z-ai/glm-5.3-flash", "openai/gpt-oss-120b", "qwen/qwen3.7-flash"]
FILL = {
    "O_ORDER": ["espresso", "latte", "matcha", "matcha latte", "taiyaki", "cake slice", "warm milk"],
    "V_GIFT": ["daisy", "fish", "basket of berries", "shiny acorn"],
    "D_EVIDENCE": ["Energy Drink Can", "Pudding Cup", "Game Controller", "Coffee Grounds", "Scale", "Flour Handprint",
                   "Timer Setting", "Cream Drop", "Mocha's Apron", "Ramen Wrapper", "Egg Carton", "Flour Trail"],
    "R_PATTERN": ["top", "middle", "bottom", "off the walls"],
}
HE = re.compile(r"(^|[ \"])(he|him|his)(?![a-z])", re.I)


def teacher_lines(spk, sit):
    out = []
    for d in ("raw", "raw2"):
        p = os.path.join(ROOT, "content", "distill", d, f"{spk}__{sit}.json")
        if os.path.exists(p):
            out += json.load(open(p))
    return out


def fill_value(sit, rng):
    """The in-game value for this situation's placeholder (one per line, shared by the line and its situation)."""
    for key, pool in FILL.items():
        if sit.startswith(key):
            return rng.choice(pool)
    return str(rng.randint(2, 12))


def fill(text, value):
    return text.replace("{item}", value).replace("{dir}", value).replace("{n}", value)


def words(s):
    return [w for w in (t.strip(".,!?;:~()\"-").lower() for t in s.split()) if w]


def bigrams(s):
    w = words(s)
    return list(zip(w, w[1:]))


def build(tag):
    rng = random.Random(11)
    out_dir = os.path.join(ROOT, "build", "quality", tag)
    os.makedirs(out_dir, exist_ok=True)
    corpus = [l for spk, sit, _ in SITS for l in teacher_lines(spk, sit)]
    corpus_set = set(corpus)
    seen_bi = collections.Counter(b for l in corpus for b in bigrams(l))
    rows = collections.defaultdict(list)
    for line in open(os.path.join(ROOT, "build", "quality", "student.tsv"), encoding="utf-8"):
        spk, sit, att, ok, text = line.rstrip("\n").split("\t", 4)
        spk, sit = SPK[int(spk)], SIT[int(sit)]
        ok = ok == "1" and not (spk == "SHIO" and sit.startswith("D_") and HE.search(text))   # the detective's gate
        rows[(spk, sit)].append((ok, text))
    items = []
    for (spk, sit), attempts in rows.items():
        for ok, text in attempts:
            items.append({"src": "student_pass" if ok else "student_fail", "spk": spk, "sit": sit, "raw": text})
        for text in rng.sample(teacher_lines(spk, sit), 2):
            items.append({"src": "teacher", "spk": spk, "sit": sit, "raw": text})
        passes = sum(ok for ok, _ in attempts)
        for it in items[-(len(attempts) + 2):]:
            it["pass_rate"] = passes / len(attempts)
    for i, it in enumerate(rng.sample(items, len(items))):
        it["id"] = f"q{i:04d}"
        it["value"] = fill_value(it["sit"], rng)
        it["shown"] = fill(it["raw"], it["value"])
        it["situation"] = fill(DESC[(it["spk"], it["sit"])], it["value"])
        bi = bigrams(it["raw"])
        it["unseen_bigrams"] = sum(1 for b in bi if not seen_bi[b])
        it["copy"] = it["raw"] in corpus_set
        it["chars"] = len(it["shown"])
    items.sort(key=lambda it: it["id"])
    json.dump(items, open(os.path.join(out_dir, "items.json"), "w"), indent=1)
    c = collections.Counter(it["src"] for it in items)
    print(f"build: {len(items)} lines -> build/quality/{tag}/items.json  {dict(c)}")


SPLIT = re.compile(r"(?<!Mr\.)(?<!Mrs\.)(?<!Ms\.)(?<=[.!?~])\s+(?=[A-Z\"'])")
SPLIT_LOWER = re.compile(r"(?<=[.!?~])\s+(?=[A-Za-z\"'])")      # kitten thoughts are lowercase


def units(spk, sit):
    """Openers (first sentence of a multi-sentence teacher line) and rests (everything after it), with the line
    each came from."""
    op, rest = [], []
    split = SPLIT_LOWER if spk == "KITTEN" else SPLIT
    for i, line in enumerate(teacher_lines(spk, sit)):
        parts = split.split(line)
        if len(parts) > 1:
            op.append((i, parts[0]))
            rest.append((i, " ".join(parts[1:])))
    return op, rest


KEYS = re.compile(r"\{(item|n|dir)\}")


def compositions(spk, sit, rng, n):
    """n random opener+rest pairs from different teacher lines that fit the dialogue box and keep exactly one
    placeholder when the situation has one."""
    op, rest = units(spk, sit)
    need = len(KEYS.findall(DESC[(spk, sit)])) > 0
    out, tries = [], 0
    if not op or not rest:
        return out
    while len(out) < n and tries < 2000:
        tries += 1
        (a, x), (b, y) = rng.choice(op), rng.choice(rest)
        line = f"{x} {y}"
        if a == b or len(line) > 88 or len(KEYS.findall(line)) != (1 if need else 0) or line in out:
            continue
        out.append(line)
    return out


def compose(tag):
    """Evaluation set of composed lines: one random composition and the best of four by NekoLM's confidence."""
    import torch
    import torch.nn.functional as F
    from nekolm import NekoLM, Tokenizer, CKPT, NL
    model = NekoLM()
    model.load_state_dict(torch.load(CKPT))
    model.eval()
    tk = Tokenizer()
    rng = random.Random(23)

    def lp(spk, sit, text):
        seq = [NL, TAGS["speakers"][spk], TAGS["situations"][sit]] + tk.encode(text.encode()) + [NL]
        with torch.no_grad():
            logp = F.log_softmax(model(torch.tensor([seq[:-1]]))[0], -1)
        v = [float(logp[i, seq[i + 1]]) for i in range(2, len(seq) - 1)]
        return sum(v) / len(v)

    items = []
    for spk, sit, _ in SITS:
        cands = compositions(spk, sit, rng, 5)
        if len(cands) < 5:
            continue
        items.append({"src": "compose_random", "spk": spk, "sit": sit, "raw": cands[0]})
        best = max(cands[1:], key=lambda c: lp(spk, sit, c))
        items.append({"src": "compose_ranked", "spk": spk, "sit": sit, "raw": best})
    d = os.path.join(ROOT, "build", "quality", tag)
    os.makedirs(d, exist_ok=True)
    for i, it in enumerate(rng.sample(items, len(items))):
        it["id"] = f"c{i:04d}"
        it["value"] = fill_value(it["sit"], rng)
        it["shown"] = fill(it["raw"], it["value"])
        it["situation"] = fill(DESC[(it["spk"], it["sit"])], it["value"])
        it["unseen_bigrams"], it["copy"], it["chars"], it["pass_rate"] = 0, False, len(it["shown"]), 1.0
    items.sort(key=lambda it: it["id"])
    json.dump(items, open(os.path.join(d, "items.json"), "w"), indent=1)
    print(f"compose: {len(items)} lines -> build/quality/{tag}/items.json ({len(items) // 2} situations)")


def vowel_sound(v):
    """Mirror of vowel_sound in src/nekolm.c."""
    if v[:1].lower() in ("a", "e", "i", "o", "u"):
        return True
    return v[:1] == "8" or (v[:1] == "1" and v[1:2] in ("1", "8") and not v[2:3].isdigit())


def c_fill(src, value, articles=True):
    """Mirror of nlm_fill in src/nekolm.c: replace the placeholder, capitalising a value that starts a sentence and
    matching the article before it ("an espresso"). articles=False reproduces the fill from before that fix."""
    out = ""
    i = 0
    while i < len(src):
        m = re.match(r"\{(item|n|dir)\}", src[i:])
        if m:
            a_before = (len(out) >= 2 and out[-1] == " " and out[-2] in "aA"
                        and (len(out) == 2 or not out[-3].isalpha()))
            an_before = (len(out) >= 3 and out[-1] == " " and out[-2] == "n" and out[-3] in "aA"
                         and (len(out) == 3 or not out[-4].isalpha()))
            if articles and a_before and vowel_sound(value):
                out = out[:-1] + "n "
            elif articles and an_before and not vowel_sound(value):
                out = out[:-2] + " "
            start = not out or (len(out) >= 2 and out[-1] == " " and (out[-2] in "!?" or
                                                                     (out[-2] == "." and (len(out) < 3 or out[-3] != "."))))
            v = value[0].upper() + value[1:] if start and value[:1].islower() else value
            out += v
            i += len(m.group(0))
        else:
            out += src[i]
            i += 1
    return out


def buildc(tag):
    """Evaluation set from the game's own composer output (tools/composeeval.sh -> build/quality/composer.tsv)."""
    rng = random.Random(31)
    items = []
    for line in open(os.path.join(ROOT, "build", "quality", "composer.tsv"), encoding="utf-8"):
        spk, sit, text = line.rstrip("\n").split("\t", 2)
        items.append({"src": "composer_c", "spk": SPK[int(spk)], "sit": SIT[int(sit)], "raw": text})
    d = os.path.join(ROOT, "build", "quality", tag)
    os.makedirs(d, exist_ok=True)
    for i, it in enumerate(rng.sample(items, len(items))):
        it["id"] = f"g{i:04d}"
        it["value"] = fill_value(it["sit"], rng)
        it["shown"] = c_fill(it["raw"], it["value"])
        it["situation"] = fill(DESC[(it["spk"], it["sit"])], it["value"])
        it["unseen_bigrams"], it["copy"], it["chars"], it["pass_rate"] = 0, False, len(it["shown"]), 1.0
    items.sort(key=lambda it: it["id"])
    json.dump(items, open(os.path.join(d, "items.json"), "w"), indent=1)
    print(f"buildc: {len(items)} lines -> build/quality/{tag}/items.json")


def score(tag):
    """Mean token log-probability of every line under the trained model (float), as a confidence feature."""
    import torch
    import torch.nn.functional as F
    from nekolm import NekoLM, Tokenizer, CKPT, NL
    model = NekoLM()
    model.load_state_dict(torch.load(CKPT))
    model.eval()
    tk = Tokenizer()
    path = os.path.join(ROOT, "build", "quality", tag, "items.json")
    items = json.load(open(path))
    with torch.no_grad():
        for it in items:
            seq = [NL, TAGS["speakers"][it["spk"]], TAGS["situations"][it["sit"]]] + tk.encode(it["raw"].encode()) + [NL]
            x = torch.tensor([seq[:-1]])
            logp = F.log_softmax(model(x)[0], -1)
            lp = [float(logp[i, seq[i + 1]]) for i in range(2, len(seq) - 1)]   # the line and its end, not the tags
            it["lp"] = sum(lp) / len(lp)
            it["lp_min"] = min(lp)
    json.dump(items, open(path, "w"), indent=1)
    for s in ("teacher", "student_pass", "student_fail"):
        v = [it["lp"] for it in items if it["src"] == s]
        print(f"score: {s:13s} mean log-prob per token {sum(v) / len(v):.3f}")


# ---------------------------------------------------------------- the judge panel
SYSTEM = """You rate short lines of dialogue from a cozy pixel-art game about cat demi-humans. Every line comes with
the speaker's description, the situation, and the line exactly as the player sees it. Rate each line on three scales:
- sense: 2 = clear, it makes complete sense; 1 = understandable but awkward, vague or slightly off; 0 = does not make
  sense (garbled, self-contradictory or meaningless).
- fits: 2 = clearly a response to this situation; 1 = loosely related or generic; 0 = unrelated or wrong for it.
- voice: 2 = sounds like this speaker; 1 = neutral, anyone could say it; 0 = contradicts the description.
Playful style, puns, cat mannerisms and short fragments are intended; judge whether the words mean something.
Reply with JSON only: {"ratings": [{"id": "...", "sense": 0, "fits": 0, "voice": 0, "why": "under 15 words"}]}
with one entry per id, in the order given."""


def ask(model, user, system=None, tag="quality-judge", effort="low"):
    """One pinned-model call; returns (content, reasoning, provider, served model). effort None = reasoning off."""
    body = {"model": model, "temperature": 0, "max_tokens": 12000,
            "messages": [{"role": "system", "content": system or SYSTEM}, {"role": "user", "content": user}],
            "reasoning": {"effort": effort} if effort else {"enabled": False},
            "response_format": {"type": "json_object"}, "usage": {"include": True}}
    req = urllib.request.Request("https://openrouter.ai/api/v1/chat/completions", data=json.dumps(body).encode(),
                                 headers={"Authorization": "Bearer " + _key(), "Content-Type": "application/json",
                                          "X-Title": "Neko Arcade NekoLM quality check"})
    for attempt in range(4):
        try:
            with urllib.request.urlopen(req, timeout=300) as r:
                j = json.load(r)
            break
        except Exception as e:  # rate limits and upstream hiccups: back off and retry
            code = getattr(e, "code", None)
            if attempt == 3 or code not in (None, 429, 500, 502, 503, 504):
                raise
            time.sleep(8 * (attempt + 1))
    msg = j["choices"][0]["message"]
    u = j.get("usage", {})
    with _log_lock, open(LOG, "a", encoding="utf-8") as f:
        f.write(json.dumps({"t": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()), "tag": tag,
                            "model": j.get("model"), "in": u.get("prompt_tokens"), "out": u.get("completion_tokens"),
                            "cost": u.get("cost")}) + "\n")
    return msg.get("content") or "", msg.get("reasoning") or "", j.get("provider"), j.get("model")


def judge(tag):
    d = os.path.join(ROOT, "build", "quality", tag)
    items = json.load(open(os.path.join(d, "items.json")))
    by_spk = collections.defaultdict(list)
    for it in items:
        by_spk[it["spk"]].append(it)
    batches = []
    for spk, its in sorted(by_spk.items()):
        for i in range(0, len(its), 24):
            batches.append((spk, its[i:i + 24]))
    lock = threading.Lock()

    def run(model, b, spk, its):
        path = os.path.join(d, f"judge_{model.replace('/', '_')}.jsonl")
        user = f"Speaker: {spk.replace('CUST_', 'customer ').title()}. {VOICES[spk]}\n\n" + "\n".join(
            f'id: {it["id"]}\nsituation: {it["situation"]}\nline: "{it["shown"]}"\n' for it in its)
        want = {it["id"] for it in its}
        for attempt in range(2):
            content, reasoning, provider, served = ask(model, user)
            try:
                got = {r["id"]: r for r in json.loads(content)["ratings"] if r.get("id") in want
                       and all(r.get(k) in (0, 1, 2) for k in ("sense", "fits", "voice"))}
            except Exception:
                got = {}
            if len(got) == len(want) or attempt == 1:
                break
        with lock, open(path, "a", encoding="utf-8") as f:
            f.write(json.dumps({"batch": b, "model": served, "provider": provider, "ratings": list(got.values()),
                                "missing": sorted(want - set(got)), "reasoning": reasoning[:4000]}) + "\n")
        return model, b, len(got), len(want)

    jobs = []
    for model in JUDGES:
        path = os.path.join(d, f"judge_{model.replace('/', '_')}.jsonl")
        done = {json.loads(l)["batch"] for l in open(path)} if os.path.exists(path) else set()
        jobs += [(model, b, spk, its) for b, (spk, its) in enumerate(batches)
                 if b not in done and b < int(os.environ.get("QLIMIT", "9999"))]
    print(f"judge: {len(batches)} batches x {len(JUDGES)} judges, {len(jobs)} to run; spend so far ${spent():.4f}")
    with cf.ThreadPoolExecutor(max_workers=9) as pool:
        for model, b, n, want in pool.map(lambda a: run(*a), jobs):
            if n < want:
                print(f"  {model} batch {b}: {n}/{want} rated")
    print(f"judge: done; spend so far ${spent():.4f}")


# ---------------------------------------------------------------- report
def median(v):
    v = sorted(v)
    return v[len(v) // 2]


def load(tag):
    d = os.path.join(ROOT, "build", "quality", tag)
    items = {it["id"]: it for it in json.load(open(os.path.join(d, "items.json")))}
    for model in JUDGES:
        path = os.path.join(d, f"judge_{model.replace('/', '_')}.jsonl")
        for line in open(path) if os.path.exists(path) else []:
            rec = json.loads(line)
            for r in rec["ratings"]:
                items[r["id"]].setdefault("j", {})[model] = r
    for it in items.values():
        j = it.get("j", {})
        if len(j) >= 2:
            for k in ("sense", "fits", "voice"):
                it[k] = median([r[k] for r in j.values()])
    return list(items.values())


def group(sit):
    for pre, name in (("V_", "village"), ("O_", "cafe customers"), ("C_", "cafe (Mocha)"), ("D_", "detective (Shio)"),
                      ("R_", "rival (Mako)"), ("K_", "kittens")):
        if sit.startswith(pre):
            return name
    return "hub (Nia)"


def pct(n, d):
    return f"{100 * n / d:.0f}%" if d else "-"


def report(tag):
    items = load(tag)
    out = [f"# NekoLM quality check ({tag})\n", f"{len(items)} lines, judged blind by {', '.join(JUDGES)}.\n"]
    order = ["teacher", "student_pass", "student_fail", "compose_random", "compose_ranked"]
    srcs = sorted({it["src"] for it in items}, key=lambda s: order.index(s) if s in order else 99)
    shown = [s for s in srcs if s not in ("teacher", "student_fail")]
    out.append("## Per judge: share of lines that make complete sense (sense = 2) / do not make sense (sense = 0)\n")
    out.append("| judge | " + " | ".join(srcs) + " |\n|---|" + "---|" * len(srcs) + "\n")
    for model in JUDGES + ["median of 3"]:
        cells = []
        for s in srcs:
            rs = [it["j"][model] if model in it.get("j", {}) else None for it in items if it["src"] == s] \
                if model != "median of 3" else [it if "sense" in it else None for it in items if it["src"] == s]
            rs = [r for r in rs if r]
            cells.append(f"{pct(sum(r['sense'] == 2 for r in rs), len(rs))} / {pct(sum(r['sense'] == 0 for r in rs), len(rs))}"
                         f" (n={len(rs)})")
        out.append(f"| {model} | " + " | ".join(cells) + " |\n")
    rated = [it for it in items if "sense" in it]
    out.append("\n## Median of 3 by scale\n\n| source | sense 2 | sense 0 | fits 2 | fits 0 | voice 2 | voice 0 |\n|---|---|---|---|---|---|---|\n")
    for s in srcs:
        rs = [it for it in rated if it["src"] == s]
        out.append(f"| {s} | " + " | ".join(pct(sum(it[k] == v for it in rs), len(rs))
                                              for k in ("sense", "fits", "voice") for v in (2, 0)) + " |\n")
    out.append("\n## By part of the game (median of 3): make sense / do not\n\n| part | " + " | ".join(srcs)
               + " |\n|---|" + "---|" * len(srcs) + "\n")
    for g in sorted({group(it["sit"]) for it in rated}):
        cells = []
        for s in srcs:
            rs = [it for it in rated if group(it["sit"]) == g and it["src"] == s]
            cells.append(f"{pct(sum(i['sense'] == 2 for i in rs), len(rs))} / {pct(sum(i['sense'] == 0 for i in rs), len(rs))}")
        out.append(f"| {g} | " + " | ".join(cells) + " |\n")
    st = [it for it in rated if it["src"].startswith("student")]
    if st:
        out.append("\n## Unseen word pairs vs nonsense (student lines, median of 3)\n\n| word pairs the teacher never used | lines | make sense | do not |\n|---|---|---|---|\n")
        for lo, hi, name in ((0, 0, "0"), (1, 1, "1"), (2, 2, "2"), (3, 99, "3+")):
            b = [it for it in st if lo <= it["unseen_bigrams"] <= hi]
            out.append(f"| {name} | {len(b)} | {pct(sum(i['sense'] == 2 for i in b), len(b))} | {pct(sum(i['sense'] == 0 for i in b), len(b))} |\n")
    for s in shown:
        out.append(f"\n## Worst situations for {s} (mean sense)\n\n| speaker / situation | mean sense | lines |\n|---|---|---|\n")
        by = collections.defaultdict(list)
        for it in rated:
            if it["src"] == s:
                by[(it["spk"], it["sit"])].append(it["sense"])
        for k, v in sorted(by.items(), key=lambda kv: sum(kv[1]) / len(kv[1]))[:12]:
            out.append(f"| {k[0]} / {k[1]} | {sum(v) / len(v):.2f} | {len(v)} |\n")
        out.append(f"\n## {s} lines the judges call nonsense (median sense 0), with each judge's reason\n\n")
        for it in [it for it in rated if it["src"] == s and it["sense"] == 0][:25]:
            why = "; ".join(f"{m.split('/')[1]}: {r.get('why', '')}" for m, r in it["j"].items())
            out.append(f"- **{it['spk']} / {it['sit']}**: \"{it['shown']}\"  \n  _{why}_\n")
    agree = []
    for a in range(len(JUDGES)):
        for b in range(a + 1, len(JUDGES)):
            both = [it for it in items if JUDGES[a] in it.get("j", {}) and JUDGES[b] in it.get("j", {})]
            same = sum(it["j"][JUDGES[a]]["sense"] == it["j"][JUDGES[b]]["sense"] for it in both)
            agree.append(f"{JUDGES[a].split('/')[1]} vs {JUDGES[b].split('/')[1]}: {pct(same, len(both))} (n={len(both)})")
    out.append("\n## Judge agreement on sense (exact)\n\n" + "\n".join(f"- {a}" for a in agree) + "\n")
    path = os.path.join(ROOT, "build", "quality", tag, "report.md")
    open(path, "w", encoding="utf-8").write("".join(out))
    print("".join(out))


if __name__ == "__main__":
    {"build": build, "buildc": buildc, "compose": compose, "score": score, "judge": judge,
     "report": report}[sys.argv[1]](sys.argv[2])
