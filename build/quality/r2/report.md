# NekoLM quality check (r2)
328 lines, judged blind by z-ai/glm-5.3-flash, openai/gpt-oss-120b, qwen/qwen3.7-flash.
## Per judge: share of lines that make complete sense (sense = 2) / do not make sense (sense = 0)
| judge | compose_random | compose_ranked |
|---|---|---|
| z-ai/glm-5.3-flash | 63% / 1% (n=164) | 67% / 1% (n=163) |
| openai/gpt-oss-120b | 91% / 1% (n=164) | 94% / 0% (n=164) |
| qwen/qwen3.7-flash | 71% / 4% (n=164) | 79% / 2% (n=164) |
| median of 3 | 77% / 0% (n=164) | 82% / 1% (n=164) |

## Median of 3 by scale

| source | sense 2 | sense 0 | fits 2 | fits 0 | voice 2 | voice 0 |
|---|---|---|---|---|---|---|
| compose_random | 77% | 0% | 85% | 0% | 88% | 1% |
| compose_ranked | 82% | 1% | 90% | 0% | 86% | 0% |

## By part of the game (median of 3): make sense / do not

| part | compose_random | compose_ranked |
|---|---|---|
| cafe (Mocha) | 62% / 0% | 75% / 0% |
| cafe customers | 82% / 0% | 88% / 0% |
| detective (Shio) | 62% / 0% | 75% / 0% |
| hub (Nia) | 77% / 0% | 100% / 0% |
| kittens | 60% / 0% | 80% / 0% |
| rival (Mako) | 85% / 0% | 69% / 8% |
| village | 78% / 0% | 79% / 0% |

## Worst situations for compose_random (mean sense)

| speaker / situation | mean sense | lines |
|---|---|---|
| CUST_PERKY / O_ORDER | 1.00 | 1 |
| MOCHA / V_GOSSIP_KITTENS | 1.00 | 1 |
| KURO / V_SMALLTALK | 1.00 | 1 |
| MOCHA / C_SLOW | 1.00 | 1 |
| SHIO / D_ODD | 1.00 | 1 |
| NIA / V_GOSSIP_CAFE | 1.00 | 1 |
| SHIO / V_NIGHT | 1.00 | 1 |
| MAKO / V_GOSSIP_KITTENS | 1.00 | 1 |
| SUZU / V_GIFT_OK | 1.00 | 1 |
| CUST_GRUMPY / O_WAIT | 1.00 | 1 |
| SUZU / V_SMALLTALK | 1.00 | 1 |
| KURO / V_GIFT_OK | 1.00 | 1 |

## compose_random lines the judges call nonsense (median sense 0), with each judge's reason


## Worst situations for compose_ranked (mean sense)

| speaker / situation | mean sense | lines |
|---|---|---|
| MAKO / R_SHE_SCORES | 0.00 | 1 |
| MOCHA / C_OPEN | 1.00 | 1 |
| CUST_FOX / O_REVIEW1 | 1.00 | 1 |
| KURO / V_GOSSIP_FISH | 1.00 | 1 |
| CUST_PERKY / O_WAIT | 1.00 | 1 |
| SHIO / D_INTRO | 1.00 | 1 |
| KURO / V_GOSSIP_RIVAL_WIN | 1.00 | 1 |
| KURO / V_GIFT_OK | 1.00 | 1 |
| SHIO / V_SMALLTALK | 1.00 | 1 |
| SHIO / D_IDLE | 1.00 | 1 |
| MAKO / R_WIN | 1.00 | 1 |
| KURO / V_HELLO_FRIEND | 1.00 | 1 |

## compose_ranked lines the judges call nonsense (median sense 0), with each judge's reason

- **MAKO / R_SHE_SCORES**: "That was a textbook shot. Neither do I."  
  _glm-5.3-flash: 'Neither do I' answers nothing; garbled fragment; gpt-oss-120b: Awkward phrasing, but fits goal comment and voice; qwen3.7-flash: Second sentence is nonsensical and contradicts the first._

## Judge agreement on sense (exact)

- glm-5.3-flash vs gpt-oss-120b: 69% (n=327)
- glm-5.3-flash vs qwen3.7-flash: 75% (n=327)
- gpt-oss-120b vs qwen3.7-flash: 75% (n=328)
