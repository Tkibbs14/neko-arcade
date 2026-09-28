#!/usr/bin/env python3
"""Fine-tune NekoLM for its job on the device: choosing which continuation best follows an opener.

  rank_train.py train [STEPS] [LAMBDA]   start from build/nekolm_base.pt (build/nekolm.pt before the first ship),
                                         learn from the labellers' labels (content/rank/labels__*.jsonl, or the
                                         ones named in env LABELLERS), keep the best step -> build/nekolm_rank.pt
  rank_train.py check [CKPT]             the label-set metrics for a checkpoint (default: the shipped model)

The device (src/live.c) ranks candidates by the mean log-probability per token of the continuation and the line end,
given (line start, speaker, situation, opener). That exact score is what is trained here, so the C engine needs new
weights and nothing else. Loss: pairwise (RankNet) on the score, for every two candidates the labels rank differently,
plus LAMBDA x the ordinary language-model loss on the teacher's lines, so the model keeps its grip on the language.
One group in ten is held out by id to choose the step; the judging panel's pool is never seen here (rank_labels.py
excludes its groups), so a later panel run on it is an independent test."""
import glob, json, math, os, random, sys, time, zlib
import torch
import torch.nn.functional as F

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import nekolm  # noqa: E402
from nekolm import NekoLM, NL, V, load_examples, batch as lm_batch  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LABELS = os.path.join(ROOT, "content", "rank")                  # labels__<labeller>.jsonl files (rank_labels.py)
OUT = os.environ.get("RANK_OUT") or os.path.join(ROOT, "build", "nekolm_rank.pt")
BASE = os.path.join(ROOT, "build", "nekolm_base.pt")      # the language model before any ranking fine-tune
TAGS = json.load(open(os.path.join(ROOT, "content", "distill", "tags.json")))


def load_groups():
    """Groups labelled by every chosen labeller (env LABELLERS, comma-separated file slugs; default: all of
    content/rank/labels__*.jsonl), each candidate's label averaged over the labellers."""
    bank = json.load(open(os.path.join(ROOT, "build", "compose_bank.json")))
    by = {(p["spk"], p["sit"]): p for p in bank}
    names, kind = os.environ.get("LABELLERS"), os.environ.get("LABEL_KIND", "labels")   # labels_v: sense + voice pass
    paths = ([os.path.join(LABELS, f"{kind}__{n}.jsonl") for n in names.split(",")] if names
             else sorted(glob.glob(os.path.join(LABELS, f"{kind}__*.jsonl"))))
    target = os.environ.get("LABEL_TARGET", "sense")      # "both": rank by sense + voice, the bank's approval rule
    per = []
    for path in paths:
        per.append({g["id"]: g for g in map(json.loads, open(path))})
    out = []
    for gid, g in per[0].items():
        if not all(gid in lab and lab[gid]["rests"] == g["rests"] for lab in per):
            continue
        p = by[(g["spk"], g["sit"])]
        prefix = [NL, TAGS["speakers"][g["spk"]], TAGS["situations"][g["sit"]]] + p["op"][g["op"]][2]
        out.append({"id": gid, "np": len(prefix), "seqs": [prefix + p["rest"][r][2] + [NL] for r in g["rests"]],
                    "y": [sum(lab[gid]["sense"][k] + (lab[gid]["voice"][k] if target == "both" else 0)
                               for lab in per) / len(per) for k in range(6)]})
    print(f"labels from {', '.join(os.path.basename(p).split('__')[1][:-6] for p in paths)} ({kind}, target {target}): "
          f"{len(out)} groups")
    return out


def scores(model, gs, drop=0.0):
    """Mean log-probability per token of each candidate's continuation and line end, as src/live.c computes it."""
    seqs = [s for g in gs for s in g["seqs"]]
    nps = [g["np"] for g in gs for _ in g["seqs"]]
    tb = max(len(s) for s in seqs) - 1
    x = torch.zeros(len(seqs), tb, dtype=torch.long)
    y = torch.zeros(len(seqs), tb, dtype=torch.long)
    m = torch.zeros(len(seqs), tb)
    for i, (s, n) in enumerate(zip(seqs, nps)):
        x[i, :len(s) - 1] = torch.tensor(s[:-1])
        y[i, :len(s) - 1] = torch.tensor(s[1:])
        m[i, n - 1:len(s) - 1] = 1
    lp = F.log_softmax(model(x, drop=drop), -1).gather(-1, y.unsqueeze(-1)).squeeze(-1)
    return ((lp * m).sum(1) / m.sum(1)).view(len(gs), -1)


def rank_loss(s, y, alpha):
    d = s.unsqueeze(2) - s.unsqueeze(1)                                  # s_i - s_j
    w = (y.unsqueeze(2) - y.unsqueeze(1)).clamp(min=0).float()          # label gap where y_i > y_j
    return (w * F.softplus(-alpha * d)).sum() / w.sum().clamp(min=1)


def metrics(model, gs):
    """best3: mean label of the pick among the first three candidates (the device draws three at random);
    best6: among all six; top3: share of groups where that pick is as good as the best of the three; acc: pairwise
    order accuracy."""
    model.eval()
    with torch.no_grad():
        S = torch.cat([scores(model, gs[i:i + 48]) for i in range(0, len(gs), 48)])
    Y = torch.tensor([g["y"] for g in gs], dtype=torch.float)
    p3 = Y[:, :3].gather(1, S[:, :3].argmax(1, keepdim=True)).squeeze(1)
    p6 = Y.gather(1, S.argmax(1, keepdim=True)).squeeze(1)
    d, w = S.unsqueeze(2) - S.unsqueeze(1), Y.unsqueeze(2) > Y.unsqueeze(1)
    return {"best3": p3.mean().item(), "best6": p6.mean().item(),
            "top3": (p3 == Y[:, :3].max(1).values).float().mean().item(),
            "acc": (d[w] > 0).float().mean().item()}


def lm_val(model, ex):
    model.eval()
    rs = random.getstate()
    random.seed(99)
    with torch.no_grad():
        v = sum(F.cross_entropy(model(x).reshape(-1, V), y.reshape(-1), ignore_index=-1).item()
                for x, y in (lm_batch(ex, 256) for _ in range(4))) / 4
    random.setstate(rs)
    return v


def fmt(m):
    return " ".join(f"{k} {v:.3f}" for k, v in m.items())


def split(gs):
    val = [g for g in gs if zlib.crc32(g["id"].encode()) % 10 == 0]
    return [g for g in gs if zlib.crc32(g["id"].encode()) % 10 != 0], val


def train(steps=600, lam=1.0, lr_max=3e-4, gb=24, lb=48):
    torch.manual_seed(1)
    random.seed(1)
    tr, val = split(load_groups())
    ex = load_examples()
    random.shuffle(ex)
    lm_hold, lm_tr = ex[:1000], ex[1000:]
    Y = torch.tensor([g["y"] for g in val], dtype=torch.float)
    print(f"{len(tr)} training groups, {len(val)} held out; held-out reference: random pick {Y.mean():.3f}, "
          f"best of 3 by the labels {Y[:, :3].max(1).values.mean():.3f}, best of 6 {Y.max(1).values.mean():.3f}")
    model = NekoLM()
    model.load_state_dict(torch.load(BASE if os.path.exists(BASE) else nekolm.CKPT))
    base = metrics(model, val)
    print(f"step     0 held-out {fmt(base)}  lm {lm_val(model, lm_hold):.3f}", flush=True)
    alpha = torch.nn.Parameter(torch.tensor(5.0))
    opt = torch.optim.AdamW([{"params": model.parameters()}, {"params": [alpha], "weight_decay": 0.0}],
                            lr=lr_max, betas=(0.9, 0.98), weight_decay=0.05)
    best, t0 = base["acc"], time.time()
    for step in range(1, steps + 1):
        lr = lr_max * min(1.0, step / 50) * (0.1 + 0.9 * 0.5 * (1 + math.cos(math.pi * step / steps)))
        for g in opt.param_groups:
            g["lr"] = lr
        model.train()
        bg = random.sample(tr, gb)
        rl = rank_loss(scores(model, bg, drop=0.1), torch.tensor([g["y"] for g in bg]), alpha)
        x, y = lm_batch(lm_tr, lb)
        ll = F.cross_entropy(model(x, drop=0.1).reshape(-1, V), y.reshape(-1), ignore_index=-1)
        loss = rl + lam * ll
        opt.zero_grad()
        loss.backward()
        torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
        opt.step()
        if step % 50 == 0 or step == steps:
            m = metrics(model, val)
            flag = ""
            if m["acc"] > best:
                best = m["acc"]
                torch.save(model.state_dict(), OUT)
                flag = " *saved"
            print(f"step {step:5d} rank {rl.item():.3f} lm {ll.item():.3f} alpha {alpha.item():.2f} | held-out {fmt(m)} "
                  f" lm {lm_val(model, lm_hold):.3f} {time.time() - t0:.0f}s{flag}", flush=True)
    print(f"best held-out pairwise accuracy {best:.3f} (base {base['acc']:.3f})" +
          (f" -> {OUT}" if best > base["acc"] else "; nothing saved"))


def check(ckpt=None):
    tr, val = split(load_groups())
    model = NekoLM()
    model.load_state_dict(torch.load(ckpt or nekolm.CKPT))
    print(f"{ckpt or nekolm.CKPT}: train {fmt(metrics(model, tr))}\n  held-out {fmt(metrics(model, val))}")


if __name__ == "__main__":
    torch.set_num_threads(4)
    if sys.argv[1] == "train":
        train(int(sys.argv[2]) if len(sys.argv) > 2 else 600, float(sys.argv[3]) if len(sys.argv) > 3 else 1.0)
    elif sys.argv[1] == "check":
        check(sys.argv[2] if len(sys.argv) > 2 else None)
