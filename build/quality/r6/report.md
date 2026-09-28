# NekoLM quality check (r6)
676 lines, judged blind by z-ai/glm-5.3-flash, openai/gpt-oss-120b, qwen/qwen3.7-flash.
## Per judge: share of lines that make complete sense (sense = 2) / do not make sense (sense = 0)
| judge | composer_c |
|---|---|
| z-ai/glm-5.3-flash | 85% / 0% (n=675) |
| openai/gpt-oss-120b | 93% / 1% (n=676) |
| qwen/qwen3.7-flash | 88% / 1% (n=675) |
| median of 3 | 93% / 0% (n=676) |

## Median of 3 by scale

| source | sense 2 | sense 0 | fits 2 | fits 0 | voice 2 | voice 0 |
|---|---|---|---|---|---|---|
| composer_c | 93% | 0% | 95% | 0% | 96% | 0% |

## By part of the game (median of 3): make sense / do not

| part | composer_c |
|---|---|
| cafe (Mocha) | 94% / 0% |
| cafe customers | 92% / 0% |
| detective (Shio) | 94% / 0% |
| hub (Nia) | 100% / 0% |
| kittens | 89% / 0% |
| rival (Mako) | 100% / 0% |
| village | 91% / 0% |

## Worst situations for composer_c (mean sense)

| speaker / situation | mean sense | lines |
|---|---|---|
| MOCHA / V_GOSSIP_RIVAL_LOSE | 1.25 | 4 |
| MOCHA / C_OPEN | 1.50 | 4 |
| KURO / V_GIFT_MEH | 1.50 | 4 |
| KURO / V_GIFT_OK | 1.50 | 4 |
| KITTEN / K_PLAY | 1.50 | 4 |
| SHIO / V_GOSSIP_RIVAL_LOSE | 1.50 | 4 |
| CUST_DOG / O_REVIEW1 | 1.50 | 4 |
| KURO / V_GOSSIP_CAFE | 1.50 | 4 |
| CUST_SLEEPY / O_REVIEW1 | 1.50 | 4 |
| KITTEN / K_SCOLDED | 1.75 | 4 |
| NIA / V_HELLO | 1.75 | 4 |
| CUST_PERKY / O_REVIEW1 | 1.75 | 4 |

## composer_c lines the judges call nonsense (median sense 0), with each judge's reason


## Judge agreement on sense (exact)

- glm-5.3-flash vs gpt-oss-120b: 82% (n=675)
- glm-5.3-flash vs qwen3.7-flash: 83% (n=674)
- gpt-oss-120b vs qwen3.7-flash: 85% (n=675)
