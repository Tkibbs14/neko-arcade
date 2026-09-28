#!/usr/bin/env python3
"""Training labels for NekoLM's ranking job. A labeller model that is NOT on the judging panel rates six candidate
lines that share one opener, on the panel's own 0-2 "sense" scale.

  rank_labels.py train EVAL_TAG MODEL [EFFORT] [MAX_GROUPS]   groups NOT in the evaluation pool
                                                  -> content/rank/labels__<model>[_r].jsonl
  rank_labels.py eval EVAL_TAG MODEL [EFFORT]     the evaluation pool's own groups -> build/quality/TAG/labels__...
  rank_labels.py whole SPK/SIT,... MODEL          whole teacher lines of those situations, sense and voice
                                                  -> content/rank/whole__<model>.jsonl (fallback pools, mklines.py)
  rank_labels.py from LABELS_FILE MODEL [EFFORT]  the very groups another labels file rated (same candidates), into
                                                  the same folder: with --voice, voice ratings for existing pairs
                                                  (how far this labeller agrees with the panel; never trained on)
EFFORT: "off" (default) or a reasoning effort such as "low"; with reasoning, each batch's reasoning is kept next to
the labels (reasoning__....jsonl) so it can be read. Resumable. Lines are shown exactly as the player sees them."""
import concurrent.futures as cf, collections, glob, json, os, random, sys, threading

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ai import spent  # noqa: E402
from composer_lab import valid_rests, HE  # noqa: E402
from distill_spec import VOICES  # noqa: E402
from nlm_quality import DESC, JUDGES, ask, fill_value, c_fill  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SYSTEM = """You rate short lines of dialogue from a cozy pixel-art game about cat demi-humans. Each item gives the
speaker's description, the situation, and six candidate lines that all start with the same sentence. Rate each line:
sense: 2 = clear, it makes complete sense; 1 = understandable but awkward, vague or slightly off; 0 = does not make
sense (garbled, self-contradictory or meaningless). Pay attention to whether the later sentences follow from the first.
Playful style, puns, cat mannerisms and short fragments are intended; judge whether the words mean something.
Reply with JSON only: {"items": [{"id": "...", "sense": [s1, s2, s3, s4, s5, s6]}]} with one entry per item."""
SYSTEM_VOICE = """You rate short lines of dialogue from a cozy pixel-art game about cat demi-humans. Each item gives the
speaker's description, the situation, and six candidate lines that all start with the same sentence. Rate each line
on two scales:
sense: 2 = clear, it makes complete sense; 1 = understandable but awkward, vague or slightly off; 0 = does not make
sense (garbled, self-contradictory or meaningless). Pay attention to whether the later sentences follow from the first.
voice: 2 = clearly sounds like this speaker as described; 1 = neutral, anyone could say it; 0 = out of character
(contradicts the description, or the tone clashes within the line).
Playful style, puns, cat mannerisms and short fragments are intended; judge whether the words mean something.
Reply with JSON only: {"items": [{"id": "...", "sense": [s1, ..., s6], "voice": [v1, ..., v6]}]} with one entry
per item."""


SYSTEM_WHOLE = SYSTEM_VOICE.replace("six candidate lines that all start with the same sentence",
                                    "six candidate lines for that situation").replace(
    " Pay attention to whether the later sentences follow from the first.", "")


def whole_groups(sits):
    """Whole teacher lines of the given situations ("SPK/SIT,..."), six to a group, every line once."""
    from nlm_quality import teacher_lines
    rng = random.Random(123)
    groups = []
    for spec in sits.split(","):
        spk, sit = spec.split("/")
        lines = sorted(set(teacher_lines(spk, sit)))
        rng.shuffle(lines)
        for g in range(0, len(lines) - len(lines) % 6, 6):
            value = fill_value(sit, rng)
            raw = lines[g:g + 6]
            groups.append({"id": f"{spk}|{sit}|w{g // 6}", "spk": spk, "sit": sit, "raw": raw, "value": value,
                           "lines": [c_fill(t, value) for t in raw]})
    return groups


def slug(model, effort):
    return model.replace("/", "_") + ("_r" if effort else "")


def six(x):
    return isinstance(x, list) and len(x) == 6 and all(v in (0, 1, 2) for v in x)


def situation(spk, sit, value):
    return DESC[(spk, sit)].replace("{item}", value).replace("{n}", value).replace("{dir}", value)


def train_groups(bank, eval_tag, max_groups):
    held = set()                                    # every evaluation pool's groups stay out of training
    for path in glob.glob(os.path.join(ROOT, "build", "quality", "pool*", "items.json")):
        held |= {it["group"] for it in json.load(open(path))}
    rng = random.Random(99)
    groups = []
    for pool in bank:
        ban = pool["spk"] == "SHIO" and pool["sit"].startswith("D_")
        for oi in range(len(pool["op"])):
            g = f"{pool['spk']}|{pool['sit']}|{oi}"
            if g in held or (ban and HE.search(pool["op"][oi][1])):
                continue
            rests = valid_rests(pool, oi, ban)
            if len(rests) >= 6:
                value = fill_value(pool["sit"], rng)
                rs = rng.sample(rests, 6)
                groups.append({"id": g, "spk": pool["spk"], "sit": pool["sit"], "op": oi, "rests": rs, "value": value,
                               "lines": [c_fill(pool["op"][oi][1] + " " + pool["rest"][r][1], value) for r in rs]})
    rng.shuffle(groups)
    return groups[:max_groups]


def eval_groups(eval_tag):
    items = json.load(open(os.path.join(ROOT, "build", "quality", eval_tag, "items.json")))
    by = collections.defaultdict(list)
    for it in items:
        by[it["group"]].append(it)
    out = []
    for g, v in sorted(by.items()):
        v.sort(key=lambda it: it["order"])
        out.append({"id": g, "spk": v[0]["spk"], "sit": v[0]["sit"], "op": v[0]["op"], "rests": [it["rest"] for it in v],
                    "value": v[0]["value"], "lines": [it["shown"] for it in v], "items": [it["id"] for it in v]})
    return out


def main(mode, eval_tag, model, effort="off", max_groups=2400, voice=False):
    effort = None if effort == "off" else effort
    if model in JUDGES:
        raise SystemExit(f"{model} is on the judging panel; its labels would not be an independent test")
    bank = json.load(open(os.path.join(ROOT, "build", "compose_bank.json")))
    if mode == "whole":                                 # whole teacher lines, for the fallback pools
        groups, d, voice = whole_groups(eval_tag), os.path.join(ROOT, "content", "rank"), True
    elif mode == "from":                                  # exactly the groups another labels file rated
        keep = ("id", "spk", "sit", "op", "rests", "value", "lines", "items")
        groups = [{k: r[k] for k in keep if k in r} for r in map(json.loads, open(eval_tag))]
        d = os.path.dirname(os.path.abspath(eval_tag))
    elif mode == "train":
        groups, d = train_groups(bank, eval_tag, max_groups), os.path.join(ROOT, "content", "rank")
    else:
        groups, d = eval_groups(eval_tag), os.path.join(ROOT, "build", "quality", eval_tag)
    kind = "whole" if mode == "whole" else "labels_v" if voice else "labels"   # voice ratings: separate files
    out = os.path.join(d, f"{kind}__{slug(model, effort)}.jsonl")
    why = os.path.join(d, f"reasoning{'_v' if voice else ''}__{slug(model, effort)}.jsonl")
    os.makedirs(d, exist_ok=True)
    done = {json.loads(l)["id"] for l in open(out)} if os.path.exists(out) else set()
    todo = [g for g in groups if g["id"] not in done]
    print(f"labels ({mode}, {model}, reasoning {effort or 'off'}): {len(groups)} groups ({len(done)} done, "
          f"{len(todo)} to go); spend so far ${spent():.4f}", flush=True)
    lock = threading.Lock()

    def run(batch):
        user = "\n\n".join(
            f"id: {g['id']}\nspeaker: {g['spk'].replace('CUST_', 'customer ').title()}. {VOICES[g['spk']]}\n"
            f"situation: {situation(g['spk'], g['sit'], g['value'])}\n" +
            "\n".join(f"{k + 1}. \"{line}\"" for k, line in enumerate(g["lines"])) for g in batch)
        want = {g["id"] for g in batch}
        got, reasoning, provider = {}, "", None
        for attempt in range(2):
            try:
                content, reasoning, provider, served = ask(model, user, system=SYSTEM_WHOLE if mode == "whole" else
                                                          SYSTEM_VOICE if voice else SYSTEM,
                                                          tag="rank-labels", effort=effort)
                got = {x["id"]: x for x in json.loads(content)["items"] if isinstance(x, dict) and x.get("id") in want
                       and six(x.get("sense")) and (not voice or six(x.get("voice")))}
            except (Exception, SystemExit) as e:        # one bad batch must not stop the others
                print(f"  batch failed: {str(e)[:160]}", flush=True)
            if len(got) == len(want):
                break
        with lock:
            with open(out, "a") as f:
                for g in batch:
                    if g["id"] in got:
                        rec = {**g, "sense": got[g["id"]]["sense"], "provider": provider}
                        if voice:
                            rec["voice"] = got[g["id"]]["voice"]
                        f.write(json.dumps(rec) + "\n")
            if reasoning:
                with open(why, "a", encoding="utf-8") as f:
                    f.write(json.dumps({"ids": sorted(want), "provider": provider, "reasoning": reasoning[:6000]}) + "\n")
        return len(got), len(batch)

    batches = [todo[i:i + 8] for i in range(0, len(todo), 8)]
    ok = tot = 0
    with cf.ThreadPoolExecutor(max_workers=8) as pool:
        for i, (a, b) in enumerate(pool.map(run, batches)):
            ok, tot = ok + a, tot + b
            if (i + 1) % 25 == 0:
                print(f"  {i + 1}/{len(batches)} batches, {ok}/{tot} groups; spend so far ${spent():.4f}", flush=True)
    print(f"labels ({mode}, {model}): {ok}/{tot} groups labelled; spend so far ${spent():.4f}")


if __name__ == "__main__":
    voice = "--voice" in sys.argv
    args = [a for a in sys.argv[1:] if a != "--voice"]
    main(args[0], args[1], args[2], *(args[3:4]), *(int(a) for a in args[4:5]), voice=voice)
