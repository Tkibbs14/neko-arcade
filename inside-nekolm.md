# Inside NekoLM

*Neko Arcade · on-device dialogue*

NekoLM is a 926,848-parameter language model that runs in C on a handheld MIPS game stick, with no internet. It makes the characters' dialogue by choosing between sentences a large model wrote, because a model this small cannot write reliable sentences on its own.

- **13%** of lines made complete sense when NekoLM wrote freely.
- **93%** make complete sense now, **96%** are in character, and none were rated nonsense.

## How a line gets made

1. **A large model writes the material.** DeepSeek V4.1 Flash wrote 20,057 lines for 169 speaker-and-situation pairs, from Mako after you score on her to a sleepy café customer whose order came fast. Nia, Mako and Shio are written from their original character cards; every line is two or three short sentences whose later sentences still make sense after another line's first. Each line is split into an opening sentence and a continuation.
2. **Three other models approve combinations.** Openings and continuations from different lines are recombined. DeepSeek, MiniMax M3 and MiMo v2.6 Flash rate every candidate. A pair ships only if all three rate it fully sensible and MiniMax and MiMo both rate it fully in character (against the character card, where there is one): 15,445 pairs, in every one of the 169 situations.
3. **NekoLM chooses on the stick.** During play the game draws an opening and offers NekoLM up to three approved continuations. NekoLM keeps the one it scores most natural, using its log-probability per token after being fine-tuned on the labellers' ratings. The item or number in play then fills its slot, so a customer orders "an espresso".

Example, Mako after you score on her:

> "You scored on me. I am still ahead in my head."

- Opening, from one line: "You scored on me."
- Continuation, from another: "I am still ahead in my head."

## Measured

| Lines | Complete sense | In character | Nonsense |
|---|---:|---:|---:|
| NekoLM writing freely | 13% | 40% | 45% |
| Recombined, before approval | 83% | 89% | 1.5% |
| First generation, approved | 94% | 95% | 0% |
| **Now: lines written from the cards, approved** | **93%** | **96%** | **0%** |
| The large model's own first-generation lines | 91% | 95% | 1% |

Judged blind by GLM 5.3 Flash, gpt-oss-120b and Qwen 3.7 Flash, none of which wrote or approved any line; each figure is the median of the three. The two approved rows are 676 lines each produced by the stick's own C code, judged the same way: against each character's card where there is one. The other rows were judged against shorter summaries. Writing from the cards raised "in character" with all three judges (+1.0 to +3.7 points); "complete sense" moved within noise (-1.2). The judges agree with each other on 85 to 91% of lines, so part of the last few percent is a matter of judgment.

## The model

- **Size:** 926,848 parameters (4 layers, width 128, 4 attention heads)
- **Tokens:** 1,024 subword vocabulary, 64-token context
- **Math:** int8 weights, integer-only forward pass in C, bit-exact with the Python reference
- **Speed:** about 10 ms per token on the stick, cut into 9 slices inside a 6 ms per-frame budget, so the game keeps 60 fps
- **Training:** from scratch on the large model's lines on a laptop CPU, then fine-tuned to rank on about 6,000 groups of candidates rated for sense and voice

## Try it

[nakamaai.app/arcade](https://nakamaai.app/arcade/)

Click the game to start. Arrow keys move, `Z` confirms and `X` goes back; a controller or the on-screen buttons work too. Try build 05, Nekomura: walk up to a villager and press `Z`. The browser runs the same C code as the stick, compiled to WebAssembly.

## What it cannot do

- It writes no sentences of its own. Every sentence comes from the large model; what varies is which opening meets which continuation, and which one NekoLM picks.
- About one line in fourteen is still slightly vague, most often an old fisherman's proverb that stretches too far.
