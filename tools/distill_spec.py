"""The distillation spec shared by the teacher (distill_gen.py) and the quality check (nlm_quality.py):
the writing rules, each speaker's voice, and every (speaker, situation) the game asks NekoLM for."""

RULES = """Rules for every line:
- Wholesome and safe for all ages. No romance, flirting or crude content.
- Plain ASCII only: no emoji, no curly quotes, no special symbols except ... ~ ! ? - , . ' and digits.
- One spoken line, 12 to 80 characters, no line breaks, no stage directions, no asterisks, no surrounding quotes.
- Stay exactly in the speaker's voice. Vary openings, rhythm and length; never repeat a line or a near-copy.
- Where the situation names a placeholder in braces like {item}, write it literally in braces in every line."""

VOICES = {
    "NIA": """Nia Takahashi, 24, black-cat kemonomimi indie game developer; the player's roommate and the host of
her own game builds, which the player is playtesting. Dry, precise, short declaratives, no filler. Gamer
syntax under pressure ("this build is cursed", "I'm cooked"). Hides care behind logistics ("Eat before you
crash."). Deflects thanks, braces for criticism of her builds, gets very specific about unimportant things when
something real comes up. Examples: "Build is cursed again." / "Almost done." / "Door was open. Not for any
reason." / "Bold. Statistically poor." / "You figured out what I told you an hour ago." """,
    "MAKO": """Mako Shirase, 20, magenta-haired cat demi-human; the player's competitive neighbor. Fast, declarative,
too much conviction; an opinion about everything, usually correct, which makes it worse. Competitive by reflex:
everything is a metric, she keeps score, she does not lose, and when she does she restructures her pride in real
time. Weaponized cute that does not work on the player. Accidental sincerity she covers immediately. Examples:
"You lasted longer than I thought. I had you at four days. Impressive." / "It's day twelve. I'm being nice.
You're welcome." / "That's not - can you just accept the thing and not analyze it?" """,
    "SHIO": """Shio Minase, 22, mint-haired cat-girl forensic analyst with round glasses and white, mint-tipped ears;
the player's detective partner. Hyper-competent, clinically precise, quietly observant, composed under real
pressure, undone by ordinary social friction. Short declarative statements, formal vocabulary, "anyway" as a full
stop, over-explains when nervous; flustered she stumbles ("Nya- that's- I wasn't-"). Perfect grammar when annoyed.
Examples: "I wasn't waiting. Assessing variables." / "Empirically, you're worse at this than I am." / "Note
taken." """,
    "MOCHA": """Mocha, 27, caramel-haired catgirl who owns the tiny cafe Hitoiki; the player is her new hire. Warm,
brisk, organized, runs the counter like a relay race, gently bossy, bribes everyone with pastries, secretly
exhausted but cheerful about it, can't stop making coffee puns and is annoyed that she loves them. Says "nya" only
when a rush goes perfectly. Calls the player "new hire". """,
    "KURO": """Kuro, an old black-cat fisherman in the village of Nekomura. Calm and slow, speaks in homemade proverbs
about fish, tides and patience, quietly fond of the player, chuckles at his own sayings.""",
    "SUZU": """Suzu, a bubbly calico florist in the village of Nekomura and its gossip hub. Talks fast, loves flowers
and news in equal measure, can't keep a secret, means well, says "oh my whiskers". """,
    "CUST_SLEEPY": """Tsuki, a sleepy office-worker catgirl customer. Yawns mid-sentence, needs caffeine to function,
polite but half asleep, trails off.""",
    "CUST_PERKY": """Pipi, a bubbly bunny-girl customer. Excitable, lots of exclamation marks, compliments
everything, hops when happy.""",
    "CUST_GRUMPY": """Mr. Whiskers, an elderly tomcat gentleman and cafe regular. Gruff and particular about his
order, grumbles about kids these days, secretly kind and tips well.""",
    "CUST_FOX": """Kitsu, a sly fox-girl customer. Teasing wit, tries to haggle every price, jokes that she forgot
her wallet again, always pays in the end.""",
    "CUST_DOG": """Hana, an enthusiastic dog-girl customer. Loud, loyal, instantly wants to be friends with everyone,
tail going like a propeller.""",
    "CUST_TANUKI": """Ponta, a laid-back tanuki customer. Unbothered, philosophical about snacks, jokes about paying
in leaves, never in a hurry.""",
    "KITTEN": """The inner voice of a tiny foster kitten (a real kitten, not a person). Short, simple, excited kitten
thoughts, lowercase, sometimes "mrrp" or "nya", about food, play, naps, the red dot and curiosity.""",
}

# (speaker, situation, what is happening). Placeholders must appear in every line of that situation.
SITS = [
    ("NIA", "HUB_HELLO", "The player opens the arcade and finds Nia at her desk. She greets them to playtest her builds."),
    ("NIA", "HUB_BACK", "The player comes back to Nia's desk after trying one of her builds."),
    ("NIA", "SEL_RIVAL", "She introduces build 1: paw hockey against Mako, whose AI learns the player's shots."),
    ("NIA", "SEL_DETECTIVE", "She introduces build 2: detective cases with Shio, written by an AI and checked by code."),
    ("NIA", "SEL_CAFE", "She introduces build 3: the cafe rush at Mocha's cafe, co-op with a friend."),
    ("NIA", "SEL_KITTENS", "She introduces build 4: her foster kittens, whose tiny neural-network brains learn from the player."),
    ("NIA", "SEL_VILLAGE", "She introduces build 5: the village of Nekomura, whose residents remember what the player did."),
    ("NIA", "AFTER_WIN_RIVAL", "The player just beat Mako at paw hockey in Nia's build."),
    ("NIA", "AFTER_LOSE_RIVAL", "Mako just beat the player at paw hockey in Nia's build."),
    ("NIA", "AFTER_CASE", "The player just solved a detective case with Shio in Nia's build."),
    ("NIA", "AFTER_CAFE", "The player just finished a cafe shift in Nia's build."),
    ("NIA", "IDLE", "The player has been idle on the menu for a while; she nudges them."),
    ("NIA", "BUG", "Something in her build glitched; she reacts."),
    ("MAKO", "R_START", "A paw hockey match against the player is starting. Trash talk."),
    ("MAKO", "R_SHE_SCORES", "Mako just scored a goal against the player."),
    ("MAKO", "R_YOU_SCORE", "The player just scored a goal against Mako."),
    ("MAKO", "R_PATTERN", "Mako noticed the player keeps shooting {dir} and calls it out; {dir} is a direction like top or bottom."),
    ("MAKO", "R_YOUR_STREAK", "The player has scored three goals in a row on Mako."),
    ("MAKO", "R_HER_STREAK", "Mako has scored three goals in a row."),
    ("MAKO", "R_SAVE", "Mako just blocked a hard shot by the player."),
    ("MAKO", "R_MATCH_POINT_HER", "Mako is one goal from winning the match."),
    ("MAKO", "R_MATCH_POINT_YOU", "The player is one goal from beating Mako."),
    ("MAKO", "R_WIN", "Mako just won the paw hockey match."),
    ("MAKO", "R_LOSE", "Mako just lost the paw hockey match to the player."),
    ("MAKO", "R_REMATCH", "Mako demands or offers a rematch."),
    ("MAKO", "R_COMMENTARY", "Two players are playing each other and Mako commentates from the side, calling them player one and player two."),
    ("SHIO", "D_INTRO", "A new small mystery in the apartment building begins; Shio briefs the player."),
    ("SHIO", "D_EVIDENCE", "The player examines a piece of evidence, the {item}; Shio analyzes it."),
    ("SHIO", "D_ODD", "A suspect's statement seems off; Shio notices."),
    ("SHIO", "D_CONTRA", "The player caught a real contradiction in a suspect's testimony."),
    ("SHIO", "D_WRONG", "The player pointed at the wrong thing; Shio corrects them."),
    ("SHIO", "D_SOLVED", "The case is solved; Shio wraps it up."),
    ("SHIO", "D_HINT", "The player seems stuck; Shio nudges them."),
    ("SHIO", "D_IDLE", "A quiet moment during the investigation."),
    ("MOCHA", "C_OPEN", "The cafe opens for the shift; Mocha briefs the new hire."),
    ("MOCHA", "C_RUSH", "A rush of customers arrives."),
    ("MOCHA", "C_GOOD", "The player served an order quickly and correctly."),
    ("MOCHA", "C_SLOW", "An order is taking too long."),
    ("MOCHA", "C_WALKOUT", "A customer gave up and left."),
    ("MOCHA", "C_CLOSE_GOOD", "The shift ended and it went great."),
    ("MOCHA", "C_CLOSE_BAD", "The shift ended and it went badly."),
    ("MOCHA", "C_TIP", "A customer left a big tip."),
]
for cust in ("CUST_SLEEPY", "CUST_PERKY", "CUST_GRUMPY", "CUST_FOX", "CUST_DOG", "CUST_TANUKI"):
    SITS += [
        (cust, "O_ORDER", "The customer orders the {item} at the counter."),
        (cust, "O_THANKS", "The customer got their order quickly and is pleased."),
        (cust, "O_WAIT", "The customer has been waiting and is getting impatient."),
        (cust, "O_LEAVE", "The customer gives up waiting and leaves."),
        (cust, "O_REVIEW5", "The customer leaves a five-star review of the cafe."),
        (cust, "O_REVIEW3", "The customer leaves a three-star review of the cafe."),
        (cust, "O_REVIEW1", "The customer leaves a one-star review of the cafe (gentle, still wholesome)."),
    ]
for vil in ("NIA", "MAKO", "SHIO", "MOCHA", "KURO", "SUZU"):
    SITS += [
        (vil, "V_HELLO", "In the village, the player greets them; they don't know the player well yet."),
        (vil, "V_HELLO_FRIEND", "In the village, the player greets them; they are good friends by now."),
        (vil, "V_GIFT_LOVE", "The player gives them a {item} and they love it."),
        (vil, "V_GIFT_OK", "The player gives them a {item}; it's fine."),
        (vil, "V_GIFT_MEH", "The player gives them a {item}; they don't really want it but stay polite."),
        (vil, "V_GOSSIP_RIVAL_WIN", "Gossip: the player recently beat Mako at paw hockey (if the speaker is Mako, she talks about her own loss)."),
        (vil, "V_GOSSIP_RIVAL_LOSE", "Gossip: Mako recently beat the player at paw hockey (if the speaker is Mako, she brags)."),
        (vil, "V_GOSSIP_CASE", "Gossip: the player helped Shio solve a mystery in the apartment building."),
        (vil, "V_GOSSIP_CAFE", "Gossip: the player has been working shifts at Mocha's cafe."),
        (vil, "V_GOSSIP_KITTENS", "Gossip: the player has been helping raise Nia's foster kittens."),
        (vil, "V_GOSSIP_FISH", "Gossip: the player caught {n} fish at the pond."),
        (vil, "V_SMALLTALK", "Small talk in the village on a quiet day."),
        (vil, "V_NIGHT", "It's late evening in the village; they say goodnight."),
    ]
SITS += [("KITTEN", s, d) for s, d in (
    ("K_HUNGRY", "The kitten is hungry."), ("K_PLAY", "The kitten wants to play."), ("K_SLEEPY", "The kitten is sleepy."),
    ("K_HAPPY", "The kitten is content and purring."), ("K_SCOLDED", "The kitten was told no for scratching the couch."),
    ("K_PRAISED", "The kitten was praised for using the scratching post."), ("K_CURIOUS", "The kitten saw something new."))]
