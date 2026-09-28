# Neko Arcade

**First look:**
- Play it: https://nakamaai.app/arcade/ (the same C code as the stick, compiled to WebAssembly).
- How the on-device AI works, with the measurements: [inside-nekolm.md](inside-nekolm.md).
- The model on the device: `src/nekolm.c` (integer forward pass, the dot-product kernels) and `src/live.c`
  (composing a line from approved pairs).
- Training and data: `tools/nekolm.py` (tokenizer, training, int8 export), `tools/rank_train.py` (ranking
  fine-tune), `tools/rank_labels.py` and `tools/mkcompose.py` (labels and the approved bank), `tools/nlm_quality.py`
  (the judging panel; reports in `build/quality/r*/report.md`).
- The scripts assume this machine's paths (`/mnt/c/Users/tyler/...`, WSL Debian), and the steps that call
  OpenRouter read a key from a local `.env` that is not in the repo. `build/` is not tracked except the evaluation
  labels and the panel reports.

Five small catgirl game demos, framed as builds Nia is making, with you as her playtester. It runs on the M15 stick
(libretro core), in a browser on the PC, and headless for tests. All five use NekoLM, a 927k-parameter
language model distilled from a large one, which writes the characters' lines live on the device.

**Play online (shareable playtest link):** https://nakamaai.app/arcade/ (deploy with `tools/deploy-web.ps1`).
**Play on the PC:** `build/web/neko-arcade.html` (open it in a browser; no install).
**On the stick:** TreeFrogUI → the Neko Arcade folder (installed by `tools/install-card.ps1`).
Keys: arrows · **Z** = A (confirm) · **X** = B (back) · **C** = X · **V** = Y · **Enter** = Start.
Player 2: WASD · F/G/R/T · 1. Any gamepad works (A confirms, B backs, as on the stick).

| Build | What it is | The AI in it |
|---|---|---|
| Nia's desk (hub) | pick a build | Nia comments on each build and on how your last playtest went |
| 1 Rival! | paw hockey against Mako, or 2 players | Mako logs which lanes you shoot at (kept in the save) and shifts her defence; trash talk per event |
| 2 Whisker Detective | 5 kitchen mysteries with Shio: search, testimony, present the clue that breaks a lie | cases written by the large model and kept only if a logic checker proved them fair; Shio's remarks live |
| 3 Nyan Cafe | a 2-minute rush at Mocha's cafe: 5 stations, lattes = espresso/matcha + milk, don't burn the taiyaki; 1-2 baristas | every customer line (orders, thanks, grumbles, reviews) and Mocha's commentary |
| 4 Foster Kittens | Mochi, Sesame and Pudding: praise (A), say no (B), feed (X), laser dot (hold Y) | each kitten's brain is a 24-weight neural policy trained by your praise and scolding; the panel shows it change |
| 5 Nekomura | a village: talk, gather, fish, give gifts | villagers gossip about what you did in the other builds (from the save) and remember gifts |

**Build:** `tools/build.sh [native|stick|web|all]` in WSL. Tests: `tools/shot.sh` (headless screenshots),
`tools/nlmtest.sh [N] [sweep]` (C model = Python reference, speed, pass rate), `tools/stick-test.sh` (stick build on
the stick's own libraries under qemu). Card install: `tools/install-card.ps1`.

**Playtest diagnostics:** on the stick the game writes `logs/nekoarcade.log` (frame and AI timing every 10 s, late
frames, and what happened in each game); press **Select** for a live performance overlay. The browser version logs to
the console. Checks: `tools/rivalsim.sh` (Rival! feel and balance), `tools/nlm_quality.py` (the dialogue judge panel).

**Dialogue:** lines are composed on the device from the big model's sentences, written with `tools/distill_gen2.py` and the guide in `tools/distill_spec2.py` (Nia, Mako and Shio from their original character cards). Every opener + continuation pair in
the bank was rated by three large models (DeepSeek, MiniMax, MiMo; `tools/rank_labels.py`), and only pairs all
three called fully sensible, and MiniMax and MiMo both called fully in character (`--voice`), are baked in
(`tools/mkcompose.py`); NekoLM, fine-tuned on those ratings (`tools/rank_train.py`), picks among an opener's
approved continuations. A separate judging panel (GLM, gpt-oss, Qwen) measures the result on the game's own
composer output (`build/quality/r4`, `r5`); NekoLM writing freely managed 13%. Situations without approved pairs
use whole teacher lines, filtered the same way (`rank_labels.py whole`, `tools/mklines.py`).
`tools/composer_lab.py` compares ranking rules on judged pools; `tools/filltest.sh` keeps the C and Python
placeholder fills identical.

**Model speed:** the dot product has three exact kernels (plain, MIPS `madd`, split sums); the game times them at
start-up and logs the pick (`kernel ...` in the log). `tools/nlmtest-mips.sh` checks every kernel as MIPS code.

**NekoLM pipeline:** `tools/distill_gen.py` (teacher lines) → `tools/nekolm.py bpe | train | quant | export` →
`src/gen/nekolm_data.c`. On the device a generated line is shown only if it passes `nlm_ok` (every word is one the
teacher used, placeholders right, no stutter); otherwise a teacher line from `src/gen/lines_data.c` is shown.
