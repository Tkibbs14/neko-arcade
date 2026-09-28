#!/usr/bin/env python3
"""Composer lab: judge a fixed pool of candidate compositions once, then evaluate ranking rules offline.

  composer_lab.py pool TAG [OPENERS] [RESTS]  per situation, OPENERS openers x RESTS valid rests each, chosen with
                                              the game's own rules (src/live.c) from build/compose_bank.json
  composer_lab.py score TAG [CKPT]            NekoLM scores for every candidate (a checkpoint's name adds a column)
  composer_lab.py cscore TAG NAME             the game's own C scores with the exported weights (column cs_NAME)
  composer_lab.py pool TAG 3 6 fresh          a new pool from openers in no other pool and no training labels
  composer_lab.py picks TAG OUT COL,COL [K]   only the lines each score column would pick -> build/quality/OUT
  composer_lab.py duel OUT                    after judging OUT: each rule's panel rating, paired tests vs the first
  (judge with: nlm_quality.py judge TAG)
  composer_lab.py rules TAG                   which pick each ranking rule makes, and how the panel rated it

A rule is judged by the panel's median sense of the rest it picks for each (situation, opener) group; the same
groups serve every rule, so rules are compared on identical choices."""
import collections, glob, json, math, os, random, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from nlm_quality import DESC, TAGS, fill_value, c_fill, load  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HE = re.compile(r"(^|[ \"])(he|him|his)(?![a-zA-Z])", re.I)
MAXLEN, T = 88, 64


def valid_rests(pool, oi, ban_he):
    """The rests the game would accept after opener oi (same rules as choose() in src/live.c)."""
    src, text, toks = pool["op"][oi]
    out = []
    for ri, (rsrc, rtext, rtoks) in enumerate(pool["rest"]):
        if rsrc == src or len(text) + 1 + len(rtext) > MAXLEN or len(toks) + len(rtoks) + 4 > T:
            continue
        ko, kr = int("{" in text), int("{" in rtext)
        if (ko + kr != 1) if pool["key"] else (ko or kr):
            continue
        if ban_he and HE.search(rtext):
            continue
        out.append(ri)
    return out


def used_groups():
    """(situation, opener) groups already in an evaluation pool or in any labeller's training labels."""
    used = set()
    for path in glob.glob(os.path.join(ROOT, "build", "quality", "pool*", "items.json")):
        used |= {it["group"] for it in json.load(open(path))}
    for path in glob.glob(os.path.join(ROOT, "content", "rank", "labels__*.jsonl")):
        used |= {json.loads(l)["id"] for l in open(path)}
    return used


def pool_cmd(tag, n_op=3, n_rest=6, fresh=False):
    bank = json.load(open(os.path.join(ROOT, "build", "compose_bank.json")))
    rng = random.Random(77 if not fresh else 78)
    used = used_groups() if fresh else set()
    items = []
    for pool in bank:
        ban = pool["spk"] == "SHIO" and pool["sit"].startswith("D_")
        ops = [oi for oi in range(len(pool["op"])) if not (ban and HE.search(pool["op"][oi][1]))
               and f"{pool['spk']}|{pool['sit']}|{oi}" not in used]
        rng.shuffle(ops)
        taken = 0
        for oi in ops:
            rests = valid_rests(pool, oi, ban)
            if len(rests) < n_rest:
                continue
            value = fill_value(pool["sit"], rng)
            for rank, ri in enumerate(rng.sample(rests, n_rest)):
                raw = pool["op"][oi][1] + " " + pool["rest"][ri][1]
                items.append({"src": "pool", "spk": pool["spk"], "sit": pool["sit"], "group": f"{pool['spk']}|{pool['sit']}|{oi}",
                              "order": rank, "op": oi, "rest": ri, "raw": raw, "value": value,
                              "shown": c_fill(raw, value), "situation": DESC[(pool["spk"], pool["sit"])].replace("{item}", value)
                              .replace("{n}", value).replace("{dir}", value)})
            taken += 1
            if taken == n_op:
                break
    rng.shuffle(items)
    for i, it in enumerate(items):
        it["id"] = f"p{i:05d}"
        it["unseen_bigrams"], it["copy"], it["chars"], it["pass_rate"] = 0, False, len(it["shown"]), 1.0
    d = os.path.join(ROOT, "build", "quality", tag)
    os.makedirs(d, exist_ok=True)
    items.sort(key=lambda it: it["id"])
    json.dump(items, open(os.path.join(d, "items.json"), "w"), indent=1)
    print(f"pool: {len(items)} candidates in {len({it['group'] for it in items})} (situation, opener) groups")


def score_cmd(tag, ckpt=None):
    import torch
    import torch.nn.functional as F
    import nekolm
    bank = json.load(open(os.path.join(ROOT, "build", "compose_bank.json")))
    by = {(p["spk"], p["sit"]): p for p in bank}
    model = nekolm.NekoLM()
    model.load_state_dict(torch.load(ckpt or nekolm.CKPT))
    model.eval()
    col = "" if not ckpt else "_" + os.path.splitext(os.path.basename(ckpt))[0]
    path = os.path.join(ROOT, "build", "quality", tag, "items.json")
    items = json.load(open(path))

    def mean_lp(prefix, toks):
        seq = prefix + toks + [nekolm.NL]
        with torch.no_grad():
            logp = F.log_softmax(model(torch.tensor([seq[:-1]]))[0], -1)
        v = [float(logp[i, seq[i + 1]]) for i in range(len(prefix) - 1, len(seq) - 1)]
        return sum(v) / len(v)

    for it in items:
        p = by[(it["spk"], it["sit"])]
        head = [nekolm.NL, TAGS["speakers"][it["spk"]], TAGS["situations"][it["sit"]]]
        it["lp_cond" + col] = mean_lp(head + p["op"][it["op"]][2], p["rest"][it["rest"]][2])
        it["lp_base" + col] = mean_lp(head, p["rest"][it["rest"]][2])
    json.dump(items, open(path, "w"), indent=1)
    print(f"score: {len(items)} candidates scored{(' with ' + ckpt) if ckpt else ''}")


def cscore_cmd(tag, name):
    """The device's own scores for every candidate (C composer, integer NekoLM from src/gen/nekolm_data.c, via
    tools/rankeval.sh) -> column cs_NAME, so rules can be judged on exactly what the game would pick."""
    import subprocess
    path = os.path.join(ROOT, "build", "quality", tag, "items.json")
    items = json.load(open(path))
    groups = collections.defaultdict(list)
    for it in items:
        groups[it["group"]].append(it)
    order, lines = [], []
    for v in groups.values():
        if len(v) != 6:
            continue
        v.sort(key=lambda it: it["order"])
        order.append(v)
        lines.append(f"{TAGS['speakers'][v[0]['spk']]} {TAGS['situations'][v[0]['sit']]} {v[0]['op']} " +
                     " ".join(str(it["rest"]) for it in v))
    out = subprocess.run(["bash", os.path.join(ROOT, "tools", "rankeval.sh")], input="\n".join(lines) + "\n",
                         capture_output=True, text=True, check=True).stdout.split("\n")
    for v, line in zip(order, out):
        for it, s in zip(v, line.split("\t")):
            it["cs_" + name] = int(s)
    json.dump(items, open(path, "w"), indent=1)
    print(f"cscore: {len(order)} groups scored by the C engine -> cs_{name}")


def picks_cmd(tag, out_tag, cols, k=3):
    """Only the lines the given rules would pick (best of k by each score column), for the panel to judge:
    far cheaper than judging every candidate, and enough to compare the rules on the same groups."""
    items = json.load(open(os.path.join(ROOT, "build", "quality", tag, "items.json")))
    groups = collections.defaultdict(list)
    for it in items:
        groups[it["group"]].append(it)
    picks, keep = {c: {} for c in cols}, {}
    for g, v in groups.items():
        v.sort(key=lambda it: it["order"])
        for c in cols:
            it = max(v[:k], key=lambda it: it[c])
            picks[c][g] = it["id"]
            keep[it["id"]] = it
    d = os.path.join(ROOT, "build", "quality", out_tag)
    os.makedirs(d, exist_ok=True)
    json.dump(sorted(keep.values(), key=lambda it: it["id"]), open(os.path.join(d, "items.json"), "w"), indent=1)
    json.dump({"k": k, "picks": picks}, open(os.path.join(d, "picks.json"), "w"), indent=1)
    print(f"picks: {len(groups)} groups, {len(keep)} distinct picked lines -> build/quality/{out_tag}/items.json")


def duel_cmd(out_tag):
    """How the panel rated each rule's picks (from picks_cmd), with a paired test of every rule against the first."""
    rated = {it["id"]: it for it in load(out_tag) if "sense" in it}
    spec = json.load(open(os.path.join(ROOT, "build", "quality", out_tag, "picks.json")))
    cols = list(spec["picks"])
    groups = [g for g in spec["picks"][cols[0]] if all(spec["picks"][c][g] in rated for c in cols)]
    print(f"{len(groups)} groups, best of {spec['k']}, panel median")
    ok = {c: [rated[spec["picks"][c][g]]["sense"] == 2 for g in groups] for c in cols}
    for c in cols:
        bad = sum(rated[spec["picks"][c][g]]["sense"] == 0 for g in groups)
        line = f"  {c:34s} sense {100 * sum(ok[c]) / len(groups):5.1f}%   nonsense {100 * bad / len(groups):4.1f}%"
        if c != cols[0]:
            fixed = sum(b and not a for a, b in zip(ok[cols[0]], ok[c]))
            broke = sum(a and not b for a, b in zip(ok[cols[0]], ok[c]))
            n, m = fixed + broke, min(fixed, broke)
            p = min(1.0, 2 * sum(math.comb(n, i) for i in range(m + 1)) / 2 ** n) if n else 1.0
            line += f"   vs {cols[0]}: fixed {fixed}, broke {broke}, McNemar p={p:.4f}"
        print(line)


def rules_cmd(tag):
    items = [it for it in load(tag) if "sense" in it]
    groups = collections.defaultdict(list)
    for it in items:
        groups[it["group"]].append(it)
    groups = {g: sorted(v, key=lambda it: it["order"]) for g, v in groups.items() if len(v) >= 6}
    cols = sorted({k[len("lp_cond"):] for it in items for k in it if k.startswith("lp_cond")})

    def mean_j(it):
        return sum(r["sense"] for r in it["j"].values()) / len(it["j"])

    def report(name, pick):
        picks = [pick(v) for v in groups.values()]
        n = len(picks)
        print(f"  {name:44s} sense {100 * sum(p['sense'] == 2 for p in picks) / n:5.1f}%   nonsense "
              f"{100 * sum(p['sense'] == 0 for p in picks) / n:4.1f}%   mean judge {sum(mean_j(p) for p in picks) / n:.3f}")

    labellers = {}                                      # labeller -> {item id: its 0-2 label} (rank_labels.py eval)
    for path in sorted(glob.glob(os.path.join(ROOT, "build", "quality", tag, "labels__*.jsonl"))):
        lab = {}
        for line in open(path):
            rec = json.loads(line)
            lab.update(zip(rec["items"], rec["sense"]))
        labellers[os.path.basename(path)[len("labels__"):-len(".jsonl")]] = lab
    print(f"{len(groups)} groups of 6 candidates")
    allc = [it for v in groups.values() for it in v]
    print(f"  {'random pick (all candidates)':44s} sense {100 * sum(p['sense'] == 2 for p in allc) / len(allc):5.1f}%   nonsense "
          f"{100 * sum(p['sense'] == 0 for p in allc) / len(allc):4.1f}%   mean judge {sum(mean_j(p) for p in allc) / len(allc):.3f}")
    report("oracle (best of 6 by the panel)", lambda v: max(v, key=mean_j))
    full = {n: lab for n, lab in labellers.items() if all(it["id"] in lab for it in allc)}
    for name, lab in full.items():                      # what a ranker that learned this labeller perfectly would score
        agree = sum(lab[it["id"]] == it["sense"] for it in allc) / len(allc)
        print(f"  labeller {name}: label equals the panel's median on {100 * agree:.1f}% of candidates")
        for k in (3, 6):
            report(f"best of {k} by {name[:28]}", lambda v, k=k, lab=lab: max(v[:k], key=lambda it: (lab[it["id"]], it["lp_cond"])))
    if len(full) > 1:
        def mean_lab(it):
            return sum(lab[it["id"]] for lab in full.values()) / len(full)
        for k in (3, 6):
            report(f"best of {k} by the mean of {len(full)} labellers", lambda v, k=k: max(v[:k], key=lambda it: (mean_lab(it), it["lp_cond"])))
    bank = json.load(open(os.path.join(ROOT, "build", "compose_bank.json")))
    rest_words = {(p["spk"], p["sit"], ri): len(r[1].split()) for p in bank for ri, r in enumerate(p["rest"])}

    def words(it):
        return rest_words[(it["spk"], it["sit"], it["rest"])]

    rest_ntok = {(p["spk"], p["sit"], ri): len(r[2]) for p in bank for ri, r in enumerate(p["rest"])}
    report("best of 3 by length (most words)", lambda v: max(v[:3], key=lambda it: (words(it), it["lp_cond"])))
    for a in (1.25, 1.5, 2.0):                          # sum / (tokens + 1)^a: a > 1 leans toward longer rests
        report(f"best of 3 by likelihood, length power {a}",
               lambda v, a=a: max(v[:3], key=lambda it: it["lp_cond"] * (rest_ntok[(it["spk"], it["sit"], it["rest"])] + 1) ** (1 - a)))
    for c in sorted({k[3:] for it in items for k in it if k.startswith("cs_")}):
        for k in (3, 6):
            report(f"best of {k} by the C engine ({c})", lambda v, k=k, c=c: max(v[:k], key=lambda it: it["cs_" + c]))
    for c in cols:
        tagc = c or " (shipped model)"
        for k in (3, 6):
            report(f"best of {k} by likelihood{tagc}", lambda v, k=k, c=c: max(v[:k], key=lambda it: it["lp_cond" + c]))
            for beta in (0.5, 1.0):
                report(f"best of {k}, fit score beta {beta}{tagc}",
                       lambda v, k=k, c=c, b=beta: max(v[:k], key=lambda it: it["lp_cond" + c] - b * it["lp_base" + c]))


if __name__ == "__main__":
    cmd, tag = sys.argv[1], sys.argv[2]
    if cmd == "pool":
        pool_cmd(tag, *(int(a) for a in sys.argv[3:5]), fresh="fresh" in sys.argv[5:])
    elif cmd == "score":
        score_cmd(tag, sys.argv[3] if len(sys.argv) > 3 else None)
    elif cmd == "picks":
        picks_cmd(tag, sys.argv[3], sys.argv[4].split(","), *(int(a) for a in sys.argv[5:6]))
    elif cmd == "duel":
        duel_cmd(tag)
    elif cmd == "cscore":
        cscore_cmd(tag, sys.argv[3])
    elif cmd == "rules":
        rules_cmd(tag)
