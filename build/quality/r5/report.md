# NekoLM quality check (r5)
676 lines, judged blind by z-ai/glm-5.3-flash, openai/gpt-oss-120b, qwen/qwen3.7-flash.
## Per judge: share of lines that make complete sense (sense = 2) / do not make sense (sense = 0)
| judge | composer_c |
|---|---|
| z-ai/glm-5.3-flash | 88% / 0% (n=676) |
| openai/gpt-oss-120b | 96% / 0% (n=676) |
| qwen/qwen3.7-flash | 92% / 0% (n=674) |
| median of 3 | 95% / 0% (n=676) |

## Median of 3 by scale

| source | sense 2 | sense 0 | fits 2 | fits 0 | voice 2 | voice 0 |
|---|---|---|---|---|---|---|
| composer_c | 95% | 0% | 97% | 0% | 96% | 0% |

## By part of the game (median of 3): make sense / do not

| part | composer_c |
|---|---|
| cafe (Mocha) | 100% / 0% |
| cafe customers | 92% / 0% |
| detective (Shio) | 100% / 0% |
| hub (Nia) | 100% / 0% |
| kittens | 96% / 0% |
| rival (Mako) | 96% / 0% |
| village | 95% / 0% |

## Worst situations for composer_c (mean sense)

| speaker / situation | mean sense | lines |
|---|---|---|
| CUST_GRUMPY / O_LEAVE | 1.50 | 4 |
| CUST_PERKY / O_REVIEW1 | 1.50 | 4 |
| CUST_FOX / O_WAIT | 1.50 | 4 |
| CUST_GRUMPY / O_WAIT | 1.50 | 4 |
| CUST_GRUMPY / O_REVIEW3 | 1.50 | 4 |
| KURO / V_GOSSIP_RIVAL_LOSE | 1.50 | 4 |
| KITTEN / K_SCOLDED | 1.75 | 4 |
| NIA / V_GIFT_LOVE | 1.75 | 4 |
| MAKO / V_GIFT_MEH | 1.75 | 4 |
| CUST_PERKY / O_REVIEW3 | 1.75 | 4 |
| NIA / V_SMALLTALK | 1.75 | 4 |
| CUST_DOG / O_WAIT | 1.75 | 4 |

## composer_c lines the judges call nonsense (median sense 0), with each judge's reason


## Judge agreement on sense (exact)

- glm-5.3-flash vs gpt-oss-120b: 85% (n=676)
- glm-5.3-flash vs qwen3.7-flash: 86% (n=674)
- gpt-oss-120b vs qwen3.7-flash: 91% (n=674)
