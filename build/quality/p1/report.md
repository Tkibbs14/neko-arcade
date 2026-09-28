# NekoLM quality check (p1)
1080 lines, judged blind by z-ai/glm-5.3-flash, openai/gpt-oss-120b, qwen/qwen3.7-flash.
## Per judge: share of lines that make complete sense (sense = 2) / do not make sense (sense = 0)
| judge | flash | pro | old |
|---|---|---|---|
| z-ai/glm-5.3-flash | 86% / 0% (n=360) | 75% / 1% (n=353) | 90% / 0% (n=357) |
| openai/gpt-oss-120b | 98% / 0% (n=360) | 95% / 1% (n=360) | 98% / 0% (n=360) |
| qwen/qwen3.7-flash | 89% / 1% (n=360) | 83% / 2% (n=360) | 92% / 1% (n=360) |
| median of 3 | 94% / 0% (n=360) | 89% / 1% (n=360) | 96% / 0% (n=360) |

## Median of 3 by scale

| source | sense 2 | sense 0 | fits 2 | fits 0 | voice 2 | voice 0 |
|---|---|---|---|---|---|---|
| flash | 94% | 0% | 97% | 0% | 96% | 0% |
| pro | 89% | 1% | 93% | 0% | 95% | 0% |
| old | 96% | 0% | 94% | 0% | 84% | 0% |

## By part of the game (median of 3): make sense / do not

| part | flash | pro | old |
|---|---|---|---|
| cafe (Mocha) | 93% / 0% | 90% / 0% | 100% / 0% |
| cafe customers | 97% / 0% | 94% / 1% | 100% / 0% |
| kittens | 97% / 0% | 90% / 0% | 100% / 0% |
| rival (Mako) | 100% / 0% | 100% / 0% | 97% / 0% |
| village | 90% / 1% | 82% / 1% | 91% / 0% |

## Worst situations for flash (mean sense)

| speaker / situation | mean sense | lines |
|---|---|---|
| SUZU / V_GIFT_LOVE | 1.53 | 30 |
| MOCHA / C_GOOD | 1.93 | 30 |
| CUST_PERKY / O_LEAVE | 1.93 | 30 |
| SHIO / V_GIFT_LOVE | 1.97 | 30 |
| SUZU / V_GIFT_OK | 1.97 | 30 |
| CUST_FOX / O_ORDER | 1.97 | 30 |
| CUST_DOG / O_REVIEW5 | 1.97 | 30 |
| KITTEN / K_HAPPY | 1.97 | 30 |
| KURO / V_GOSSIP_CAFE | 2.00 | 30 |
| MAKO / R_YOU_SCORE | 2.00 | 30 |
| NIA / V_SMALLTALK | 2.00 | 30 |
| CUST_TANUKI / O_THANKS | 2.00 | 30 |

## flash lines the judges call nonsense (median sense 0), with each judge's reason

- **SUZU / V_GIFT_LOVE**: "A fish just for showing you the roses? Kuro hums, has anyone told him?"  
  _glm-5.3-flash: Garbled: 'just for showing you the roses' and 'Kuro hums' unclear.; gpt-oss-120b: awkward phrasing, still fits fish gift; qwen3.7-flash: Grammatically broken and nonsensical. Does not make sense._

## Worst situations for pro (mean sense)

| speaker / situation | mean sense | lines |
|---|---|---|
| SUZU / V_GIFT_OK | 1.57 | 30 |
| KURO / V_GOSSIP_CAFE | 1.80 | 30 |
| CUST_FOX / O_ORDER | 1.80 | 30 |
| NIA / V_SMALLTALK | 1.87 | 30 |
| SUZU / V_GIFT_LOVE | 1.90 | 30 |
| KITTEN / K_HAPPY | 1.90 | 30 |
| MOCHA / C_GOOD | 1.90 | 30 |
| CUST_PERKY / O_LEAVE | 1.93 | 30 |
| SHIO / V_GIFT_LOVE | 1.93 | 30 |
| CUST_TANUKI / O_THANKS | 2.00 | 30 |
| MAKO / R_YOU_SCORE | 2.00 | 30 |
| CUST_DOG / O_REVIEW5 | 2.00 | 30 |

## pro lines the judges call nonsense (median sense 0), with each judge's reason

- **SUZU / V_GIFT_OK**: "A shiny acorn for my shelf? Miso tried a new napping spot today."  
  _glm-5.3-flash: Miso napping tangent ignores the acorn entirely.; gpt-oss-120b: irrelevant Miso comment; qwen3.7-flash: Miso's napping spot is completely unrelated to acorns._
- **CUST_FOX / O_ORDER**: "The latte is the key to my heart. Doors are wallets."  
  _glm-5.3-flash: 'Doors are wallets' is incoherent wordplay; gpt-oss-120b: nonsensical metaphor, unrelated voice; qwen3.7-flash: Nonsensical metaphor. 'Doors are wallets' makes no logical sense._

## Worst situations for old (mean sense)

| speaker / situation | mean sense | lines |
|---|---|---|
| NIA / V_SMALLTALK | 1.77 | 30 |
| KURO / V_GOSSIP_CAFE | 1.90 | 30 |
| SUZU / V_GIFT_OK | 1.90 | 30 |
| SHIO / V_GIFT_LOVE | 1.97 | 30 |
| MAKO / R_YOU_SCORE | 1.97 | 30 |
| CUST_FOX / O_ORDER | 2.00 | 30 |
| MOCHA / C_GOOD | 2.00 | 30 |
| SUZU / V_GIFT_LOVE | 2.00 | 30 |
| CUST_PERKY / O_LEAVE | 2.00 | 30 |
| CUST_DOG / O_REVIEW5 | 2.00 | 30 |
| KITTEN / K_HAPPY | 2.00 | 30 |
| CUST_TANUKI / O_THANKS | 2.00 | 30 |

## old lines the judges call nonsense (median sense 0), with each judge's reason


## Judge agreement on sense (exact)

- glm-5.3-flash vs gpt-oss-120b: 83% (n=1070)
- glm-5.3-flash vs qwen3.7-flash: 83% (n=1070)
- gpt-oss-120b vs qwen3.7-flash: 87% (n=1080)
