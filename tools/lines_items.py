#!/usr/bin/env python3
"""Whole teacher lines from several sources, side by side, for the judging panel (tools/nlm_quality.py judge).

  lines_items.py TAG PER SPK/SIT,... NAME=DIR [NAME=DIR ...]
    DIR is a folder of <speaker>__<situation>.json line lists; the name "old" (no DIR) means the first
    generation (content/distill/raw and raw2). PER lines are drawn at random per situation and source, and every
    line is shown as the player would see it (placeholder filled, as in src/nekolm.c).
Writes build/quality/TAG/items.json; the report then compares the sources column by column."""
import glob, json, os, random, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from nlm_quality import DESC, c_fill, fill, fill_value, teacher_lines  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def source_lines(src, spk, sit):
    if src == "old":
        return teacher_lines(spk, sit)
    out = []
    for path in sorted(glob.glob(os.path.join(src, "**", f"{spk}__{sit}.json"), recursive=True)):
        out += json.load(open(path))
    return out


def main():
    tag, per, sits = sys.argv[1], int(sys.argv[2]), [s.split("/") for s in sys.argv[3].split(",")]
    sources = []
    for arg in sys.argv[4:]:
        name, _, d = arg.partition("=")
        sources.append((name, "old" if not d else d if os.path.isabs(d) else os.path.join(ROOT, d)))
    rng = random.Random(41)
    items = []
    for spk, sit in sits:
        for name, src in sources:
            lines = sorted(set(source_lines(src, spk, sit)))
            for raw in rng.sample(lines, min(per, len(lines))):
                value = fill_value(sit, rng)
                items.append({"src": name, "spk": spk, "sit": sit, "raw": raw, "value": value,
                              "shown": c_fill(raw, value), "situation": fill(DESC[(spk, sit)], value)})
    rng.shuffle(items)
    for i, it in enumerate(items):
        it["id"] = f"t{i:05d}"
        it["unseen_bigrams"], it["copy"], it["chars"], it["pass_rate"] = 0, False, len(it["shown"]), 1.0
    items.sort(key=lambda it: it["id"])
    d = os.path.join(ROOT, "build", "quality", tag)
    os.makedirs(d, exist_ok=True)
    json.dump(items, open(os.path.join(d, "items.json"), "w"), indent=1)
    counts = {n: sum(it["src"] == n for it in items) for n, _ in sources}
    print(f"lines_items: {len(items)} lines -> build/quality/{tag}/items.json {counts}")


if __name__ == "__main__":
    main()
