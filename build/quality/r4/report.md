# NekoLM quality check (r4)
676 lines, judged blind by z-ai/glm-5.3-flash, openai/gpt-oss-120b, qwen/qwen3.7-flash.
## Per judge: share of lines that make complete sense (sense = 2) / do not make sense (sense = 0)
| judge | composer_c |
|---|---|
| z-ai/glm-5.3-flash | 88% / 0% (n=676) |
| openai/gpt-oss-120b | 98% / 0% (n=676) |
| qwen/qwen3.7-flash | 92% / 0% (n=675) |
| median of 3 | 96% / 0% (n=676) |

## Median of 3 by scale

| source | sense 2 | sense 0 | fits 2 | fits 0 | voice 2 | voice 0 |
|---|---|---|---|---|---|---|
| composer_c | 96% | 0% | 98% | 0% | 94% | 0% |

## By part of the game (median of 3): make sense / do not

| part | composer_c |
|---|---|
| cafe (Mocha) | 94% / 0% |
| cafe customers | 98% / 0% |
| detective (Shio) | 97% / 0% |
| hub (Nia) | 98% / 0% |
| kittens | 93% / 0% |
| rival (Mako) | 94% / 0% |
| village | 96% / 0% |

## Worst situations for composer_c (mean sense)

| speaker / situation | mean sense | lines |
|---|---|---|
| KITTEN / K_CURIOUS | 1.50 | 4 |
| CUST_GRUMPY / O_LEAVE | 1.50 | 4 |
| KURO / V_GOSSIP_CASE | 1.50 | 4 |
| MOCHA / C_SLOW | 1.75 | 4 |
| NIA / AFTER_LOSE_RIVAL | 1.75 | 4 |
| KURO / V_GIFT_MEH | 1.75 | 4 |
| MOCHA / V_HELLO | 1.75 | 4 |
| NIA / V_GIFT_LOVE | 1.75 | 4 |
| KURO / V_GOSSIP_FISH | 1.75 | 4 |
| NIA / V_SMALLTALK | 1.75 | 4 |
| SHIO / D_CONTRA | 1.75 | 4 |
| SUZU / V_GOSSIP_FISH | 1.75 | 4 |

## composer_c lines the judges call nonsense (median sense 0), with each judge's reason


## Judge agreement on sense (exact)

- glm-5.3-flash vs gpt-oss-120b: 88% (n=676)
- glm-5.3-flash vs qwen3.7-flash: 86% (n=675)
- gpt-oss-120b vs qwen3.7-flash: 92% (n=675)
