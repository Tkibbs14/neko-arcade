#!/usr/bin/env python3
"""NekoLM: the tiny in-game language model (student) distilled from the teacher's lines.

  nekolm.py bpe             learn the subword vocabulary (byte pair merges) from the teacher's lines
  nekolm.py train [steps]   train the float model on content/distill/dataset.txt (CPU)
  nekolm.py quant           int8-quantize it and run the integer-only simulation on samples
  nekolm.py sample [n]      sample lines from the integer model for every situation (quality check)
  nekolm.py export          write src/gen/nekolm_data.c/.h (weights) + build/nekolm_vectors.txt (C test vectors)

Tokens: 0-255 are bytes (10 = end of line, 0x80+ speaker tags, 0xA0+ situation tags), 256+ are byte pair
merges learned inside words, so the model spends its capacity on word order instead of spelling.
The integer forward pass here is the reference for src/nekolm.c: every rounding step, shift and clip is
mirrored exactly (C integer division truncates toward zero; >> is an arithmetic shift on our compilers).
Fixed-point formats: residual stream Q12 int32; matmul inputs Q8 int16; weights int8 per output row with a
Q20 row multiplier; attention q/k Q7, v Q8; attention weights Q12; logits Q8.
"""
import collections, json, math, os, random, re, sys, time
import torch
import torch.nn as nn
import torch.nn.functional as F

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(ROOT, "content", "distill", "dataset.txt")
TAGS = os.path.join(ROOT, "content", "distill", "tags.json")
BPE = os.path.join(ROOT, "content", "distill", "bpe.json")
CKPT = os.path.join(ROOT, "build", "nekolm.pt")
QPATH = os.path.join(ROOT, "build", "nekolm_q.pt")

MERGES = 768
V, T, D, H, L = 256 + MERGES, 64, 128, 4, 4
HD = D // H
NL = 10
torch.manual_seed(1)
random.seed(1)


# ---------------------------------------------------------------- subword vocabulary
CHUNK = re.compile(rb"\{[a-z]+\}| ?[A-Za-z']+| ?[0-9]+| ?[^ A-Za-z0-9']+| +")


def text_lines():
    return [raw[2:] for raw in open(DATA, "rb").read().split(b"\n") if len(raw) > 2]


def bpe_learn():
    words = collections.Counter(tuple(ch) for line in text_lines() for ch in CHUNK.findall(line))
    merges = []
    for m in range(MERGES):
        pairs = collections.Counter()
        for w, c in words.items():
            for p in zip(w, w[1:]):
                pairs[p] += c
        if not pairs:
            break
        (a, b), n = pairs.most_common(1)[0]
        new = 256 + m
        merges.append([a, b])
        nxt = collections.Counter()
        for w, c in words.items():
            out, i = [], 0
            while i < len(w):
                if i + 1 < len(w) and w[i] == a and w[i + 1] == b:
                    out.append(new)
                    i += 2
                else:
                    out.append(w[i])
                    i += 1
            nxt[tuple(out)] += c
        words = nxt
    json.dump(merges, open(BPE, "w"))
    strs = tok_strings(merges)
    print(f"bpe: {len(merges)} merges; longest tokens: {sorted(strs[256:], key=len)[-8:]}")


def tok_strings(merges=None):
    merges = merges if merges is not None else json.load(open(BPE))
    s = [bytes([i]) for i in range(256)]
    for a, b in merges:
        s.append(s[a] + s[b])
    return s


class Tokenizer:
    def __init__(self):
        self.rank = {tuple(p): i for i, p in enumerate(json.load(open(BPE)))}
        self.cache = {}

    def chunk(self, ch):
        if ch in self.cache:
            return self.cache[ch]
        w = list(ch)
        while len(w) > 1:
            r, i = min((self.rank.get((w[i], w[i + 1]), 1 << 30), i) for i in range(len(w) - 1))
            if r == 1 << 30:
                break
            w[i:i + 2] = [256 + r]
        self.cache[ch] = w
        return w

    def encode(self, text):
        return [t for ch in CHUNK.findall(text) for t in self.chunk(ch)]


# ---------------------------------------------------------------- float model
class RMSNorm(nn.Module):
    def __init__(self, d):
        super().__init__()
        self.g = nn.Parameter(torch.ones(d))

    def forward(self, x):
        return x * torch.rsqrt(x.pow(2).mean(-1, keepdim=True) + 1e-5) * self.g


class Block(nn.Module):
    def __init__(self):
        super().__init__()
        self.n1, self.n2 = RMSNorm(D), RMSNorm(D)
        self.qkv = nn.Linear(D, 3 * D, bias=False)
        self.proj = nn.Linear(D, D, bias=False)
        self.fc = nn.Linear(D, 4 * D, bias=False)
        self.out = nn.Linear(4 * D, D, bias=False)

    def forward(self, x, drop):
        B, Tn, C = x.shape
        q, k, v = self.qkv(self.n1(x)).split(C, 2)
        q, k, v = (t.view(B, Tn, H, HD).transpose(1, 2) for t in (q, k, v))
        a = F.scaled_dot_product_attention(q, k, v, is_causal=True, dropout_p=drop if self.training else 0.0)
        x = x + F.dropout(self.proj(a.transpose(1, 2).reshape(B, Tn, C)), drop, self.training)
        x = x + F.dropout(self.out(F.relu(self.fc(self.n2(x)))), drop, self.training)
        return x


class NekoLM(nn.Module):
    def __init__(self):
        super().__init__()
        self.tok = nn.Embedding(V, D)
        self.pos = nn.Embedding(T, D)
        self.blocks = nn.ModuleList(Block() for _ in range(L))
        self.nf = RMSNorm(D)
        nn.init.normal_(self.tok.weight, std=0.05)
        nn.init.normal_(self.pos.weight, std=0.02)

    def forward(self, idx, drop=0.0):
        x = self.tok(idx) + self.pos(torch.arange(idx.shape[1]))
        for b in self.blocks:
            x = b(x, drop)
        return self.nf(x) @ self.tok.weight.T


# ---------------------------------------------------------------- data
def load_examples():
    tk, ex = Tokenizer(), []
    for raw in open(DATA, "rb").read().split(b"\n"):
        if len(raw) < 3:
            continue
        seq = [NL, raw[0], raw[1]] + tk.encode(raw[2:]) + [NL]
        if len(seq) <= T + 1:
            ex.append(seq)
    return ex


def batch(examples, n):
    seqs = [random.choice(examples) for _ in range(n)]
    tb = max(len(s) for s in seqs) - 1                # pad only to the longest line in this batch
    xs = torch.zeros(n, tb, dtype=torch.long)
    ys = torch.full((n, tb), -1, dtype=torch.long)
    for i, seq in enumerate(seqs):
        inp, tgt = seq[:-1], seq[1:]
        xs[i, :len(inp)] = torch.tensor(inp)
        ys[i, :len(tgt)] = torch.tensor(tgt)
        ys[i, 0] = ys[i, 1] = -1          # the tags are given, not predicted
    return xs, ys


def train(steps):
    ex = load_examples()
    random.shuffle(ex)
    nval = max(200, len(ex) // 20)
    val, tr = ex[:nval], ex[nval:]
    model = NekoLM()
    ntok = sum(len(s) - 4 for s in ex)
    print(f"params {sum(p.numel() for p in model.parameters()):,}; train {len(tr)} lines, val {len(val)}; "
          f"{ntok / len(ex):.1f} tokens per line")
    opt = torch.optim.AdamW(model.parameters(), lr=3e-3, betas=(0.9, 0.98), weight_decay=0.05)
    best, t0 = 1e9, time.time()
    for step in range(1, steps + 1):
        lr = 3e-3 * min(1.0, step / 200) * (0.1 + 0.9 * 0.5 * (1 + math.cos(math.pi * step / steps)))
        for g in opt.param_groups:
            g["lr"] = lr
        model.train()
        x, y = batch(tr, 128)
        loss = F.cross_entropy(model(x, drop=0.15).reshape(-1, V), y.reshape(-1), ignore_index=-1)
        opt.zero_grad()
        loss.backward()
        torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
        opt.step()
        if step % 250 == 0 or step == steps:
            model.eval()
            with torch.no_grad():
                rs = random.getstate()                 # the same validation batches every time
                random.seed(99)
                vl = sum(F.cross_entropy(model(xv).reshape(-1, V), yv.reshape(-1), ignore_index=-1).item()
                         for xv, yv in (batch(val, 256) for _ in range(4))) / 4
                random.setstate(rs)
            flag = ""
            if vl < best:
                best = vl
                torch.save(model.state_dict(), CKPT)
                flag = " *saved"
            print(f"step {step:5d} train {loss.item():.3f} val {vl:.3f} lr {lr:.5f} {time.time() - t0:.0f}s{flag}", flush=True)
    print(f"best val loss {best:.3f} -> {CKPT}")


# ---------------------------------------------------------------- quantization
def q_rows(w):
    """int8 per output row + Q20 row multiplier."""
    s = w.abs().amax(dim=1).clamp(min=1e-8) / 127.0
    wq = torch.round(w / s[:, None]).clamp(-127, 127).to(torch.int64)
    m = torch.round(s * (1 << 20)).to(torch.int64)
    return wq, m


def quantize():
    model = NekoLM()
    model.load_state_dict(torch.load(CKPT))
    sd = model.state_dict()
    q = {}
    q["tok"], q["tok_m"] = q_rows(sd["tok.weight"])
    q["pos"] = torch.round(sd["pos.weight"] * 4096).to(torch.int64)
    for i in range(L):
        p = f"blocks.{i}."
        wqkv = sd[p + "qkv.weight"].clone()
        wqkv[:D] /= math.sqrt(HD)                      # fold the attention scale into q
        q[f"{i}.qkv"], q[f"{i}.qkv_m"] = q_rows(wqkv)
        q[f"{i}.proj"], q[f"{i}.proj_m"] = q_rows(sd[p + "proj.weight"])
        q[f"{i}.fc"], q[f"{i}.fc_m"] = q_rows(sd[p + "fc.weight"])
        q[f"{i}.out"], q[f"{i}.out_m"] = q_rows(sd[p + "out.weight"])
        q[f"{i}.g1"] = torch.round(sd[p + "n1.g"] * 4096).to(torch.int64)
        q[f"{i}.g2"] = torch.round(sd[p + "n2.g"] * 4096).to(torch.int64)
    q["gf"] = torch.round(sd["nf.g"] * 4096).to(torch.int64)
    torch.save(q, QPATH)
    return q


# ---------------------------------------------------------------- integer reference forward pass
I32 = (1 << 31) - 1
EXP = [min(32767, int(round(32768 * math.exp(-i / 256.0)))) for i in range(4096)]   # exp(-x), x in Q8, Q15 out
stats = {"max_acc": 0}


def chk(t):
    m = int(t.abs().max())
    if m > stats["max_acc"]:
        stats["max_acc"] = m
    if m > I32:
        raise OverflowError(f"int32 overflow: {m}")
    return t


def tdiv(a, b):
    return torch.div(a, b, rounding_mode="trunc")


def clip(t, lim):
    return t.clamp(-lim, lim)


def matvec(w, m, a):
    """w: int8 rows (R x C), m: Q20 row multipliers, a: Q8 int16 input (C) -> Q12 outputs (R)."""
    acc = chk(w @ a)
    return (acc * m) >> 16


def rmsnorm_q8(x, g):
    ms = int((x * x).sum()) // D                       # x is Q12 -> ms is Q24 (non-negative: floor == trunc)
    r = max(1, math.isqrt(ms + 16))
    return clip(tdiv(x * g, r * 16), 32767)


class IntLM:
    def __init__(self, q):
        self.q = q
        self.reset()

    def reset(self):
        self.k = [torch.zeros(T, D, dtype=torch.int64) for _ in range(L)]
        self.v = [torch.zeros(T, D, dtype=torch.int64) for _ in range(L)]
        self.n = 0

    def step(self, tok):
        q, pos = self.q, self.n
        x = ((q["tok"][tok] * q["tok_m"][tok]) >> 8) + q["pos"][pos]          # Q12
        for i in range(L):
            a = rmsnorm_q8(x, q[f"{i}.g1"])
            qkv = matvec(q[f"{i}.qkv"], q[f"{i}.qkv_m"], a)
            qq = clip(qkv[:D] >> 5, 8191)                                        # Q7
            self.k[i][pos] = clip(qkv[D:2 * D] >> 5, 8191)                       # Q7
            self.v[i][pos] = clip(qkv[2 * D:] >> 4, 8191)                        # Q8
            o = torch.zeros(D, dtype=torch.int64)
            for h in range(H):
                sl = slice(h * HD, (h + 1) * HD)
                sc = chk(self.k[i][:pos + 1, sl] @ qq[sl]) >> 6                  # Q14 -> Q8
                mx = int(sc.max())
                e = torch.tensor([EXP[min(4095, mx - int(s))] for s in sc], dtype=torch.int64)
                ssum = int(e.sum())
                w = tdiv(e << 12, ssum)                                          # Q12
                o[sl] = chk(w @ self.v[i][:pos + 1, sl]) >> 12                   # Q8
            x = x + matvec(q[f"{i}.proj"], q[f"{i}.proj_m"], clip(o, 32767))
            b = rmsnorm_q8(x, q[f"{i}.g2"])
            hdn = matvec(q[f"{i}.fc"], q[f"{i}.fc_m"], b)
            hdn = clip(torch.clamp(hdn, min=0) >> 4, 8191)                       # ReLU, Q12 -> Q8
            x = x + matvec(q[f"{i}.out"], q[f"{i}.out_m"], hdn)
        self.n += 1
        hf = rmsnorm_q8(x, q["gf"])
        acc = chk(q["tok"] @ hf)                                                 # int8 x Q8
        return (acc * q["tok_m"]) >> 20                                          # Q8 logits


WORD = re.compile(rb"[A-Za-z']+")
PLACE = re.compile(rb"\{[a-z]+\}")


def fnv1a(b):
    h = 2166136261
    for c in b:
        h = ((h ^ c) * 16777619) & 0xFFFFFFFF
    return h


def word_hashes():
    """Every word the teacher used (lowercase, apostrophes trimmed), as the device checks them."""
    out = set()
    for line in text_lines():
        for w in WORD.findall(PLACE.sub(b" ", line)):
            w = w.strip(b"'").lower()
            if w:
                out.add(fnv1a(w))
    return sorted(out)


def line_ok(s, key, known):
    """Mirror of nlm_ok in src/nekolm.c (s: bytes before {key} substitution)."""
    if len(s) < 8:
        return False
    for w in WORD.findall(PLACE.sub(b" ", s)):
        w = w.strip(b"'").lower()
        if w and fnv1a(w) not in known:
            return False
    places = PLACE.findall(s)
    if b"}" in PLACE.sub(b"", s) or b"{" in PLACE.sub(b"", s):
        return False
    if any(p != b"{" + key + b"}" for p in places) if key else places:
        return False
    if key and not places:
        return False
    if re.search(rb"(.)\1\1\1", s):
        return False
    words = s.split()
    if any(len(a) >= 2 and a == b for a, b in zip(words, words[1:])):
        return False
    return not any(words[i:i + 2] == words[i + 2:i + 4] for i in range(len(words) - 3))


ALLOWED = [NL] + list(range(32, 127)) + list(range(256, V))
MAXLEN = 88                                            # characters, as NLM_MAXLEN in src/nekolm.h


def sample_line(lm, spk, sit, temp_q8=384, topk=6, rng=None, strs=None):
    """temp_q8 = 256 / temperature: 384 samples at temperature 0.67, as the game does (src/nekolm.c)."""
    rng = rng or random.Random()
    strs = strs or tok_strings()
    lm.reset()
    for t in (NL, spk, sit):
        logits = lm.step(t)
    out = b""
    while lm.n < T:
        z = [(int(logits[t]) * temp_q8 >> 8, t) for t in ALLOWED]
        z.sort(reverse=True)
        z = z[:topk]
        mx = z[0][0]
        w = [EXP[min(4095, mx - v)] for v, _ in z]
        r = rng.randrange(sum(w))
        for (v, t), wt in zip(z, w):
            if r < wt:
                break
            r -= wt
        if t == NL or len(out) + len(strs[t]) > MAXLEN:
            break
        out += strs[t]
        logits = lm.step(t)
    return out.decode("ascii", "replace")


def sample_all(q, n, temp_q8=384, topk=6):
    tags = json.load(open(TAGS))
    lm = IntLM(q)
    rng = random.Random(7)
    strs = tok_strings()
    teacher = set(text_lines())
    combos = sorted({(r[0], r[1]) for r in (l for l in open(DATA, "rb").read().split(b"\n") if len(l) > 2)})
    spk_name = {v: k for k, v in tags["speakers"].items()}
    sit_name = {v: k for k, v in tags["situations"].items()}
    known = set(word_hashes())
    keys = {"D_EVIDENCE": b"item", "O_ORDER": b"item", "V_GIFT_LOVE": b"item", "V_GIFT_OK": b"item",
            "V_GIFT_MEH": b"item", "V_GOSSIP_FISH": b"n", "R_PATTERN": b"dir"}
    copies = passed = 0
    picked = combos[:: max(1, len(combos) // n)]
    for spk, sit in picked:
        line = sample_line(lm, spk, sit, temp_q8=temp_q8, topk=topk, rng=rng, strs=strs)
        copy = line.encode() in teacher
        ok = line_ok(line.encode(), keys.get(sit_name[sit]), known)
        copies += copy
        passed += ok
        print(f"{spk_name[spk]:12s} {sit_name[sit]:20s} {'=' if copy else ' '}{' ' if ok else 'x'} {line}")
    print(f"{copies}/{len(picked)} lines are verbatim teacher lines (=); {passed}/{len(picked)} pass the device "
          f"checks (x = would be replaced by a teacher line)")
    print(f"max int accumulator seen: {stats['max_acc']:,} (int32 limit {I32:,})")


# ---------------------------------------------------------------- export for C
def export(q):
    gen = os.path.join(ROOT, "src", "gen")
    os.makedirs(gen, exist_ok=True)

    def arr(name, t, ctype):
        flat = [int(v) for v in t.reshape(-1).tolist()]
        body = ",".join(str(v) for v in flat)
        return f"const {ctype} {name}[{len(flat)}] = {{{body}}};\n"

    parts = ["/* Generated by tools/nekolm.py export - NekoLM weights (int8 rows, Q20 multipliers). */\n",
             '#include "nekolm_data.h"\n']
    parts.append(arr("nlm_tok", q["tok"], "int8_t"))
    parts.append(arr("nlm_tok_m", q["tok_m"], "int32_t"))
    parts.append(arr("nlm_pos", q["pos"], "int16_t"))
    for i in range(L):
        for key, ct in (("qkv", "int8_t"), ("qkv_m", "int32_t"), ("proj", "int8_t"), ("proj_m", "int32_t"),
                        ("fc", "int8_t"), ("fc_m", "int32_t"), ("out", "int8_t"), ("out_m", "int32_t"),
                        ("g1", "int16_t"), ("g2", "int16_t")):
            parts.append(arr(f"nlm_l{i}_{key}", q[f"{i}.{key}"], ct))
    parts.append(arr("nlm_gf", q["gf"], "int16_t"))
    parts.append("const uint16_t nlm_exp[4096] = {" + ",".join(str(v) for v in EXP) + "};\n")
    strs = tok_strings()                                    # token -> text, for decoding on the device
    offs, pool = [0], b""
    for s in strs:
        pool += s
        offs.append(len(pool))
    parts.append(f"const uint16_t nlm_str_off[{V + 1}] = {{" + ",".join(map(str, offs)) + "};\n")
    parts.append(f"const uint8_t nlm_str[{len(pool)}] = {{" + ",".join(map(str, pool)) + "};\n")
    words = word_hashes()                                   # the device only shows words the teacher used
    parts.append(f"const uint32_t nlm_words[{len(words)}] = {{" + ",".join(f"{h}u" for h in words) + "};\n")
    open(os.path.join(gen, "nekolm_data.c"), "w").write("".join(parts))
    tags = json.load(open(TAGS))
    h = ["/* Generated by tools/nekolm.py export. */\n#pragma once\n#include <stdint.h>\n",
         f"#define NLM_V {V}\n#define NLM_T {T}\n#define NLM_D {D}\n#define NLM_H {H}\n#define NLM_L {L}\n"]
    for nm, ct in (("tok", "int8_t"), ("tok_m", "int32_t"), ("pos", "int16_t")):
        h.append(f"extern const {ct} nlm_{nm}[];\n")
    for i in range(L):
        for key, ct in (("qkv", "int8_t"), ("qkv_m", "int32_t"), ("proj", "int8_t"), ("proj_m", "int32_t"),
                        ("fc", "int8_t"), ("fc_m", "int32_t"), ("out", "int8_t"), ("out_m", "int32_t"),
                        ("g1", "int16_t"), ("g2", "int16_t")):
            h.append(f"extern const {ct} nlm_l{i}_{key}[];\n")
    h.append("extern const int16_t nlm_gf[];\nextern const uint16_t nlm_exp[4096];\n")
    h.append("extern const uint16_t nlm_str_off[];\nextern const uint8_t nlm_str[];\n")
    h.append(f"#define NLM_WORDS {len(words)}\nextern const uint32_t nlm_words[];\n")
    for k, v in tags["speakers"].items():
        h.append(f"#define SPK_{k} 0x{v:02X}\n")
    for k, v in tags["situations"].items():
        h.append(f"#define SIT_{k} 0x{v:02X}\n")
    open(os.path.join(gen, "nekolm_data.h"), "w").write("".join(h))
    # test vectors: logits after feeding a fixed prefix, for the C port to match exactly
    lm = IntLM(q)
    spk, sit = tags["speakers"]["MAKO"], tags["situations"]["R_YOU_SCORE"]
    seq = [NL, spk, sit] + Tokenizer().encode(b"You got lucky")
    with open(os.path.join(ROOT, "build", "nekolm_vectors.txt"), "w") as f:
        for t in seq:
            lg = lm.step(t)
            top = sorted(range(V), key=lambda i: -int(lg[i]))[:5]
            f.write(f"{t} " + " ".join(f"{i}:{int(lg[i])}" for i in top) + f" sum:{int(lg.sum())}\n")
    print(f"export: src/gen/nekolm_data.c ({os.path.getsize(os.path.join(gen, 'nekolm_data.c')) // 1024} KB), vectors ok")


if __name__ == "__main__":
    torch.set_num_threads(4)
    cmd = sys.argv[1] if len(sys.argv) > 1 else "train"
    if cmd == "bpe":
        bpe_learn()
    elif cmd == "train":
        train(int(sys.argv[2]) if len(sys.argv) > 2 else 3000)
    elif cmd == "quant":
        q = quantize()
        sample_all(q, 24)
    elif cmd == "sample":
        sample_all(torch.load(QPATH), int(sys.argv[2]) if len(sys.argv) > 2 else 40,
                   *(int(a) for a in sys.argv[3:5]))
    elif cmd == "export":
        export(torch.load(QPATH))
