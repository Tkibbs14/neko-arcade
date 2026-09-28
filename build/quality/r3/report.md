# NekoLM quality check (r3)
338 lines, judged blind by z-ai/glm-5.3-flash, openai/gpt-oss-120b, qwen/qwen3.7-flash.
## Per judge: share of lines that make complete sense (sense = 2) / do not make sense (sense = 0)
| judge | composer_c |
|---|---|
| z-ai/glm-5.3-flash | 70% / 2% (n=338) |
| openai/gpt-oss-120b | 91% / 1% (n=338) |
| qwen/qwen3.7-flash | 80% / 3% (n=338) |
| median of 3 | 83% / 1% (n=338) |

## Median of 3 by scale

| source | sense 2 | sense 0 | fits 2 | fits 0 | voice 2 | voice 0 |
|---|---|---|---|---|---|---|
| composer_c | 83% | 1% | 89% | 1% | 89% | 1% |

## By part of the game (median of 3): make sense / do not

| part | composer_c |
|---|---|
| cafe (Mocha) | 75% / 0% |
| cafe customers | 86% / 0% |
| detective (Shio) | 69% / 6% |
| hub (Nia) | 85% / 0% |
| kittens | 86% / 0% |
| rival (Mako) | 96% / 0% |
| village | 81% / 3% |

## Worst situations for composer_c (mean sense)

| speaker / situation | mean sense | lines |
|---|---|---|
| SHIO / D_ODD | 0.50 | 2 |
| KURO / V_GIFT_OK | 1.00 | 2 |
| CUST_SLEEPY / O_WAIT | 1.00 | 2 |
| NIA / V_SMALLTALK | 1.00 | 2 |
| MOCHA / V_GOSSIP_RIVAL_LOSE | 1.00 | 2 |
| KURO / V_SMALLTALK | 1.00 | 2 |
| MOCHA / V_GIFT_LOVE | 1.00 | 2 |
| CUST_FOX / O_WAIT | 1.00 | 2 |
| CUST_DOG / O_REVIEW1 | 1.00 | 2 |
| SHIO / V_GOSSIP_RIVAL_LOSE | 1.00 | 2 |
| KURO / V_GOSSIP_FISH | 1.00 | 2 |
| NIA / SEL_DETECTIVE | 1.50 | 2 |

## composer_c lines the judges call nonsense (median sense 0), with each judge's reason

- **MOCHA / V_GOSSIP_RIVAL_LOSE**: "Losing at paw hockey to Mako is fine. And for your pride."  
  _glm-5.3-flash: 'And for your pride' is garbled, meaning unclear.; gpt-oss-120b: Awkward phrasing, loosely fits gossip, voice contradicts speaker; qwen3.7-flash: Grammatically broken and nonsensical response to gossip._
- **KURO / V_SMALLTALK**: "A good story is like a good cast. He did not appreciate it."  
  _glm-5.3-flash: Story/cast metaphor collapses into non sequitur; gpt-oss-120b: awkward pronoun, loosely related; qwen3.7-flash: Nonsensical fragment; 'He did not appreciate it' breaks flow._
- **SHIO / V_GOSSIP_RIVAL_LOSE**: "Nya- I wasn't- the topic arose. Mako's was high. The rest is noise."  
  _glm-5.3-flash: Flustered stumble fits; 'Mako's was high' slightly unclear.; gpt-oss-120b: Garble, unrelated, wrong speaker.; qwen3.7-flash: Nonsensical fragments. Unrelated to Mako bragging. Voice lacks specific fluster._
- **SHIO / D_ODD**: "Nya- that's- the hall carpet has no footprints. The tide tables disagree. Anyway."  
  _glm-5.3-flash: Flustered stumble good; 'tide tables' connection unclear.; gpt-oss-120b: Nonsensical content, though stutter matches nervous Shio.; qwen3.7-flash: Garbled logic. Unrelated to suspect statement. Voice stumbles poorly._
- **NIA / V_SMALLTALK**: "I fixed the wobble. He whispered the news."  
  _glm-5.3-flash: 'He whispered the news' is garbled and unrelated.; gpt-oss-120b: Vague small‑talk, mildly fitting; qwen3.7-flash: 'He whispered the news' is nonsensical here. Garbled._

## Judge agreement on sense (exact)

- glm-5.3-flash vs gpt-oss-120b: 72% (n=338)
- glm-5.3-flash vs qwen3.7-flash: 78% (n=338)
- gpt-oss-120b vs qwen3.7-flash: 81% (n=338)
