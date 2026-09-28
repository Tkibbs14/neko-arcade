# NekoLM quality check (r1)
1144 lines, judged blind by z-ai/glm-5.3-flash, openai/gpt-oss-120b, qwen/qwen3.7-flash.
## Per judge: share of lines that make complete sense (sense = 2) / do not make sense (sense = 0)
| judge | teacher | student_pass | student_fail |
|---|---|---|---|
| z-ai/glm-5.3-flash | 91% / 0% (n=338) | 13% / 44% (n=673) | 3% / 67% (n=132) |
| openai/gpt-oss-120b | 86% / 3% (n=338) | 26% / 37% (n=674) | 6% / 71% (n=132) |
| qwen/qwen3.7-flash | 86% / 1% (n=338) | 11% / 57% (n=673) | 3% / 81% (n=132) |
| median of 3 | 91% / 1% (n=338) | 13% / 45% (n=674) | 3% / 76% (n=132) |

## Median of 3 by scale

| source | sense 2 | sense 0 | fits 2 | fits 0 | voice 2 | voice 0 |
|---|---|---|---|---|---|---|
| teacher | 91% | 1% | 98% | 0% | 95% | 1% |
| student_pass | 13% | 45% | 48% | 7% | 39% | 14% |
| student_fail | 3% | 76% | 30% | 13% | 23% | 36% |

## By part of the game (median of 3): make sense / do not

| part | teacher | student_pass | pass rate |
|---|---|---|---|
| cafe (Mocha) | 100% / 0% | 3% / 53% | 86% |
| cafe customers | 94% / 0% | 17% / 35% | 90% |
| detective (Shio) | 100% / 0% | 3% / 63% | 61% |
| hub (Nia) | 81% / 0% | 23% / 38% | 83% |
| kittens | 86% / 0% | 36% / 36% | 80% |
| rival (Mako) | 100% / 0% | 10% / 46% | 88% |
| village | 87% / 2% | 9% / 50% | 83% |

## Unseen word pairs vs nonsense (student lines, median of 3)

| word pairs the teacher never used | lines | make sense | do not |
|---|---|---|---|
| 0 | 227 | 24% | 29% |
| 1 | 251 | 12% | 46% |
| 2 | 188 | 3% | 60% |
| 3+ | 140 | 1% | 79% |

## Worst situations for the model's shown lines (student_pass, mean sense)

| speaker / situation | mean sense | lines |
|---|---|---|
| MOCHA / V_GOSSIP_RIVAL_LOSE | 0.00 | 4 |
| NIA / SEL_DETECTIVE | 0.00 | 4 |
| SHIO / V_GOSSIP_KITTENS | 0.00 | 4 |
| CUST_DOG / O_REVIEW1 | 0.00 | 4 |
| MOCHA / V_NIGHT | 0.00 | 4 |
| SHIO / D_WRONG | 0.00 | 4 |
| MAKO / V_GIFT_MEH | 0.00 | 4 |
| SUZU / V_GOSSIP_FISH | 0.00 | 4 |
| SHIO / V_GOSSIP_CAFE | 0.00 | 4 |
| CUST_GRUMPY / O_REVIEW1 | 0.00 | 4 |
| KITTEN / K_HUNGRY | 0.00 | 4 |
| CUST_FOX / O_THANKS | 0.00 | 4 |
| MAKO / R_START | 0.00 | 4 |
| NIA / IDLE | 0.00 | 4 |
| SHIO / D_CONTRA | 0.00 | 4 |
| NIA / SEL_RIVAL | 0.25 | 4 |
| SHIO / V_SMALLTALK | 0.25 | 4 |
| CUST_FOX / O_REVIEW1 | 0.25 | 4 |
| SHIO / V_GOSSIP_CASE | 0.25 | 4 |
| KITTEN / K_CURIOUS | 0.25 | 4 |

## Shown model lines the judges call nonsense (median sense 0), with one judge's reason

- **MOCHA / V_GOSSIP_RIVAL_LOSE**: "New hire, did you have a paw hockey game? I will take it, I am a surprise."  
  _glm-5.3-flash: Garbled; 'I will take it, I am a surprise' is meaningless.; gpt-oss-120b: awkward brag, not clear; qwen3.7-flash: Nonsensical grammar and meaning._
- **NIA / SEL_RIVAL**: "Build one. Paw hockey. Mako's AI is not a moment."  
  _glm-5.3-flash: Intro structure right, but 'not a moment' is nonsense.; gpt-oss-120b: Nonsensical phrase, not a declarative log; qwen3.7-flash: 'Not a moment' is meaningless in context. Awkward phrasing._
- **MAKO / V_GOSSIP_FISH**: "If you caught 10 fish, I had a whole moment. In that, you?"  
  _glm-5.3-flash: 'I had a whole moment. In that, you?' is incoherent; gpt-oss-120b: Vague meaning, loosely related fish gossip, generic voice; qwen3.7-flash: Nonsensical phrasing; does not convey a clear reaction to gossip._
- **MAKO / R_HER_STREAK**: "I am not crying. I am just the angle."  
  _glm-5.3-flash: 'I am just the angle' is meaningless; gpt-oss-120b: Nonsensical, unrelated to scoring, no matching voice; qwen3.7-flash: Meaningless fragment; fails to express emotion or context._
- **KURO / V_GOSSIP_CASE**: "You did not have a good thing. That is the whole story of you."  
  _glm-5.3-flash: Garbled insult, contradicts fondness and situation; gpt-oss-120b: nonsense, no sense; qwen3.7-flash: Nonsensical grammar and meaning; contradicts calm fisherman persona._
- **NIA / SEL_KITTENS**: "They follow them. Theirs are a sounded."  
  _glm-5.3-flash: 'Theirs are a sounded' is garbled.; gpt-oss-120b: Nonsensical, not matching foster‑kitten intro; qwen3.7-flash: 'Sounded' is not a noun here. Garbled syntax. Wrong voice._
- **MAKO / R_MATCH_POINT_HER**: "One goal. I can tell anyone how I am."  
  _glm-5.3-flash: 'I can tell anyone how I am' is garbled; gpt-oss-120b: Odd phrasing, fits imminent win, weak voice; qwen3.7-flash: Grammatically incorrect and semantically void._
- **KITTEN / K_CURIOUS**: "i touch my tail is not even"  
  _glm-5.3-flash: 'my tail is not even' is garbled; gpt-oss-120b: nonsensical sentence; qwen3.7-flash: Grammatically broken and meaningless; 'is not even' lacks object._
- **SHIO / D_INTRO**: "The card has a mile. The ceiling is not the sound."  
  _glm-5.3-flash: 'Card has a mile' is meaningless; briefing unclear.; gpt-oss-120b: nonsense, no meaning; qwen3.7-flash: Complete gibberish. No logical connection between sentences._
- **MAKO / V_NIGHT**: "Goodnight. I am not trying to say that, and I am just awake to be."  
  _glm-5.3-flash: 'Not trying to say that, just awake to be' is incoherent; gpt-oss-120b: Confusing, loosely goodnight, not Mako’s style; qwen3.7-flash: Word salad; no coherent message conveyed._
- **SUZU / V_GOSSIP_RIVAL_LOSE**: "Mako's paw hockey win made my whole match to the whole village, she willow."  
  _glm-5.3-flash: 'Made my whole match to the whole village, she willow' is garbled.; gpt-oss-120b: Nonsensical, unrelated, no character voice; qwen3.7-flash: Garbled syntax ('match to the whole village', 'she willow')._
- **NIA / V_GOSSIP_KITTENS**: "You skated a rude. I will not ask about it."  
  _glm-5.3-flash: 'Skated a rude' is garbled.; gpt-oss-120b: Garble, no meaning, not Nia’s tone; qwen3.7-flash: 'Skated a rude' is nonsense. Does not make sense._
- **NIA / SEL_DETECTIVE**: "The AI wrote a patch. The AI wrote the cross."  
  _glm-5.3-flash: 'Wrote the cross' is meaningless.; gpt-oss-120b: Nonsensical, unrelated to detective intro; qwen3.7-flash: 'Wrote the cross' is unclear/meaningless. Vague technical jargon._
- **SHIO / V_GOSSIP_KITTENS**: "They fell asleep on the sound. I returned it. That is a most."  
  _glm-5.3-flash: 'Asleep on the sound... that is a most' garbled.; gpt-oss-120b: gibberish, no sense; qwen3.7-flash: 'Fell asleep on the sound' is meaningless. 'Most' is incomplete._
- **MOCHA / V_GOSSIP_CASE**: "Shio keeps calling her latte art. I did not panic. You did."  
  _glm-5.3-flash: Disjointed; latte art and panic don't connect to mystery.; gpt-oss-120b: odd mystery line, not relevant; qwen3.7-flash: Completely unrelated and garbled._
- **MOCHA / V_GIFT_LOVE**: "This basket of berries is so good I might fix it. And a dusty promise."  
  _glm-5.3-flash: 'Might fix it. And a dusty promise' is garbled.; gpt-oss-120b: nonsensical, vague gratitude; qwen3.7-flash: Meaningless phrases 'fix it', 'dusty promise'._
- **MAKO / V_GOSSIP_RIVAL_WIN**: "I am not avoiding Mako's win. I am a difference."  
  _glm-5.3-flash: 'I am a difference' is meaningless; also third-person self-reference odd; gpt-oss-120b: Nonsensical, unrelated loss, no voice; qwen3.7-flash: Nonsensical response to losing; 'difference' makes no sense here._
- **MAKO / R_YOU_SCORE**: "That was your favorite. I am not ahead."  
  _glm-5.3-flash: 'That was your favorite. I am not ahead' is confusing; gpt-oss-120b: Vague, loosely fits goal, weak voice; qwen3.7-flash: Confusing logic; 'favorite' and 'ahead' don't connect._
- **MAKO / R_COMMENTARY**: "Player one is not a setup. I'm just saying it."  
  _glm-5.3-flash: 'Player one is not a setup' makes no sense as commentary; gpt-oss-120b: Clear, fits commentary, blunt Mako voice; qwen3.7-flash: Unclear meaning; 'setup' usage is ambiguous and awkward._
- **MAKO / V_GOSSIP_FISH**: "The pond has a story for myself. You caught 12 fish. Obviously."  
  _glm-5.3-flash: 'A story for myself' is garbled; gpt-oss-120b: Clear, fits fish gossip, slight Mako tone; qwen3.7-flash: Subjective confusion; 'story for myself' is irrelevant to gossip._
- **KURO / V_GOSSIP_RIVAL_LOSE**: "I have lost the gulls. You lost the paw hockey match. The fish will not be."  
  _glm-5.3-flash: Gull and fish clauses are nonsense; gpt-oss-120b: garbled, unrelated brag; qwen3.7-flash: Garbled syntax; 'lost the gulls' makes no sense in context._
- **MAKO / V_HELLO_FRIEND**: "Oh, it's you. I was just about to come up with a bad syrup."  
  _glm-5.3-flash: 'Come up with a bad syrup' is nonsense; gpt-oss-120b: Clear, fits friendly greeting, sarcastic Mako voice; qwen3.7-flash: Random nonsense; 'bad syrup' has no contextual link._
- **KURO / V_GIFT_MEH**: "The river gives you basket of berries and a lot, and a good heart."  
  _glm-5.3-flash: Awkward grammar but polite proverbial thanks; gpt-oss-120b: nonsensical, no fit; qwen3.7-flash: Grammatically broken and confusing; lacks clear proverbial meaning._
- **NIA / SEL_RIVAL**: "Build one. Paw hockey. Mako's AI is not a file."  
  _glm-5.3-flash: Intro fine, but 'not a file' is nonsense.; gpt-oss-120b: Nonsensical, not matching intro; qwen3.7-flash: 'Not a file' is nonsensical comparison. Awkward and vague._
- **CUST_TANUKI / O_WAIT**: "I'm so patient, but my order is still normal."  
  _glm-5.3-flash: Impatience fits but 'order is still normal' is meaningless.; gpt-oss-120b: Garbled, unclear meaning despite impatience context.; qwen3.7-flash: Contradicts impatience situation. Nonsensical phrasing about order being normal._
- **SHIO / D_ODD**: "Nya- that's- the lamp was not- the stared at the street was not. Anyway."  
  _glm-5.3-flash: Intended flustered stumble; fragments mostly garbled.; gpt-oss-120b: gibberish, no clear sense; qwen3.7-flash: Stuttering leads to complete semantic collapse. Hard to parse meaning._
- **CUST_DOG / O_REVIEW1**: "I want to be good but I am not saying that is howl!"  
  _glm-5.3-flash: 'not saying that is howl' is meaningless despite dog pun; gpt-oss-120b: Clear pun, weak negative fit, dog howl; qwen3.7-flash: Nonsensical; 'howl' pun doesn't connect to being good._
- **MOCHA / V_NIGHT**: "You are the only one of a new hire. Sleep like a good silence."  
  _glm-5.3-flash: 'Sleep like a good silence' is poetic nonsense.; gpt-oss-120b: weird phrasing, loosely night; qwen3.7-flash: Grammatically broken and confusing._
- **SHIO / D_WRONG**: "You pointed tomorrow. I am not adequate."  
  _glm-5.3-flash: 'Pointed tomorrow' and 'not adequate' are garbled.; gpt-oss-120b: nonsensical correction; qwen3.7-flash: 'Pointed tomorrow' is impossible. 'Not adequate' is out of character._
- **MAKO / V_GIFT_MEH**: "You brought me a daisy. I am keeping the rest of things."  
  _glm-5.3-flash: Second clause 'keeping the rest of things' is garbled; gift acceptance works.; gpt-oss-120b: Awkward phrasing, but polite acceptance fits.; qwen3.7-flash: Grammar is broken; 'rest of things' is vague and nonsensical._
- **SUZU / V_NIGHT**: "Oh my whiskers, I will tell you everyone the milk, and you are!"  
  _glm-5.3-flash: 'Tell you everyone the milk, and you are' is gibberish, no goodnight.; gpt-oss-120b: Incoherent goodnight, no voice; qwen3.7-flash: Nonsensical ending ('tell you everyone the milk')._
- **MAKO / V_HELLO**: "Hey. You look like you don't know why. I'm not."  
  _glm-5.3-flash: Dangling fragments, but gruff dismissive greeting suits her.; gpt-oss-120b: Nonsensical, unrelated to greeting.; qwen3.7-flash: Meaningless fragments; does not convey a clear greeting or observation._
- **CUST_DOG / O_REVIEW1**: "I gave three stars and my whole discount has a little spin."  
  _glm-5.3-flash: Says three stars in one-star scene; 'discount has a spin' nonsense; gpt-oss-120b: Confusing rating mismatch, unrelated voice; qwen3.7-flash: Contradicts situation (gave 3 stars vs 1 star) and is garbled._
- **MAKO / V_GOSSIP_CASE**: "Shio is going straight out and I am supposed to say so."  
  _glm-5.3-flash: 'Going straight out' garbled; gossip topic barely present.; gpt-oss-120b: Garble, no clear meaning.; qwen3.7-flash: Completely unrelated to gossip; sentence structure is incoherent._
- **SUZU / V_GOSSIP_CAFE**: "Oh my whiskers, you're the truth, I can you really wait!"  
  _glm-5.3-flash: 'I can you really wait' garbles the praise; cafe absent.; gpt-oss-120b: Garbage, unrelated, no voice; qwen3.7-flash: Grammar collapse ('I can you really wait')._
- **SUZU / V_GOSSIP_FISH**: "I told you the pond gave a whole miller, and she said 11 fish."  
  _glm-5.3-flash: 'Whole miller' garbles an otherwise gossipy fish report.; gpt-oss-120b: Nonsensical, unrelated, no voice; qwen3.7-flash: Confusing narrative ('pond gave a whole miller')._
- **NIA / SEL_KITTENS**: "You did not read it. They are the whole story."  
  _glm-5.3-flash: Vague, but the dry tone fits her.; gpt-oss-120b: Garble, no sense, wrong voice; qwen3.7-flash: Disjointed fragments. 'Whole story' lacks context. Garbled._
- **KITTEN / K_HAPPY**: "the tall thing is my favorite little"  
  _glm-5.3-flash: 'favorite little' trailing off makes no sense; gpt-oss-120b: vague but content; qwen3.7-flash: Incomplete thought; 'favorite little' what? Makes no sense._
- **MAKO / V_GIFT_MEH**: "I accept the shiny acorn. You are free. I am predicted with it."  
  _glm-5.3-flash: Acceptance clear; 'free' and 'predicted with it' nonsense.; gpt-oss-120b: Incoherent, no clear acceptance.; qwen3.7-flash: 'Predicted with it' is gibberish; makes no sense in this context._
- **MOCHA / V_NIGHT**: "You are a goodnight. That is a goodnight."  
  _glm-5.3-flash: Repetitive 'You are a goodnight' is meaningless.; gpt-oss-120b: meaningless repetition; qwen3.7-flash: Repetitive but understandable._

## Judge agreement on sense (exact)

- glm-5.3-flash vs gpt-oss-120b: 66% (n=1143)
- glm-5.3-flash vs qwen3.7-flash: 77% (n=1142)
- gpt-oss-120b vs qwen3.7-flash: 65% (n=1143)
