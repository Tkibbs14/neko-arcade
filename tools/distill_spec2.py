"""The teacher's writing guide for the second generation of lines (tools/distill_gen2.py).

The judges and labellers keep describing each speaker with VOICES from distill_spec.py, so ratings stay comparable
with every earlier round; this file only tells the writer more. Two goals, both from the quality panel's complaints
about the first generation (build/quality/r4, r5): lines must sound like their speaker (no sentence anyone could
say), and every line must recombine: the game joins one line's first sentence to another line's later sentences,
so a later sentence may never lean on something only its own first sentence named.

Nia, Mako and Shio come from Tyler's published character cards: their full card is part of VOICES (distill_spec.py)
and is the authority for their personality, as Tyler requires for any character with a card. The other speakers
are originals of this project; their notes extend the first-generation descriptions, with folklore checked
2026-09-27 for the two customers built on it: Wikipedia "Kitsune" (tricksters who repay their debts, fried tofu,
many tails) and Wikipedia "Japanese raccoon dog" plus the Tanuki statue article (leaves turned into money, lucky
statues outside shops, round belly). Tanuki folklore's sake is left out: the game is all ages."""
from distill_spec import SITS, VOICES  # noqa: F401  (re-exported for the generator)

RULES = """Rules for every line:
- Wholesome and safe for all ages. No romance, flirting, alcohol or crude content.
- Plain ASCII only: no emoji, no curly quotes, no special symbols except ... ~ ! ? - , . ' and digits.
- One spoken line of two or three short sentences: 8 to 15 words and under 80 characters in all, each sentence
  2 to 7 words. No line breaks, stage directions, asterisks or surrounding quotes.
- The first sentence reacts to the situation. Each later sentence stays about this same situation and must still
  make sense after a different first sentence this speaker might say here. It may refer to what every line here is
  about (the gift, the order, the goal, the player), but never to a detail that only its own first sentence names.
  A later sentence must react to the situation or follow from it; an unrelated tangent reads as nonsense.
- Every line has two or three sentences, even a proverb or a kitten thought: add a short second sentence.
- Every sentence sounds like this speaker: their interests, habits and word choices. Never write a sentence anyone
  could say, like "Thank you.", "See you later!", "Great job." or "Keep it up."
- Use a signature phrase or verbal tic in at most one line in five. Vary openings, rhythm and length; never repeat
  a line or a near-copy.
- Where the situation names a placeholder in braces like {item}, write it literally, with the braces; the game
  fills it in. Never replace it with a real value."""

WRITER = {
    "NIA": """Her source card above is the authority for her personality and voice: write from its style,
leakage, mannerisms and quote bank, in fresh words. In this game she hosts her own builds and the player
playtests them, so her work talk is about builds, bugs and playtests.""",
    "MAKO": """Her source card above is the authority for her personality and voice: write from its speech
patterns, emotional leakage and quote bank, in fresh words. In this game she is the player's rival at paw hockey
and in the village.""",
    "SHIO": """Her source card above is the authority for her personality and voice: write from its speech,
leakage, contradictions and quote bank, in fresh words. In this game she is the player's detective partner on
small mysteries in the apartment building. Write her stumble with hyphens: "Nya- that's- I wasn't-".""",
    "MOCHA": """Runs the counter like a relay race: handoffs, tags, the next leg, table numbers, tickets. Warm,
brisk and gently bossy. Bribes everyone with pastries. Makes coffee puns and is visibly annoyed that she loves them.
Cheerful about being tired ("My feet filed a complaint. Denied."). Calls the player "new hire" in some lines, not
all. Says "nya" only when a rush goes perfectly.
Examples: "Tickets up, new hire. You take pastries, I take the machine." / "My feet filed a complaint. Denied,
we have a rush." / "Table four is thrilled. Have a croissant, you earned the flaky one." """,
    "KURO": """An old fisherman who speaks slowly in homemade proverbs about fish, tides, bait, nets, weather and
patience, and now and then chuckles at one ("Heh."). Mentions his boat, the pond, the morning mist, the heron that steals his
catch. Quietly fond of the player and says so in understatement. Never hurried, never slangy, rarely exclaims.
Follow a proverb with a short plain second sentence to the player.
Examples: "A calm line catches the hungry fish. Mind your knots." / "The heron stole my breakfast again. Sit, the tide is slow
today." / "Sit a while. The tide doesn't need us yet." """,
    "SUZU": """Talks fast, loves flowers and news in equal measure: snapdragons, marigolds, sweet peas,
hydrangeas, what is blooming and who said what. Cannot keep a secret ("Don't tell anyone, but..."). Gossip is always
kind. Her flower talk and gossip always connect to what is happening right now: where a gift will go, who she will
tell about it, what the news means. "Oh my whiskers" in at most one line in five.
Examples: "A {item} for my counter! It goes right between the sweet peas." / "You caught fish at the pond? I'm
telling Mocha before lunch." / "Stand still, you have a petal on your ear." """,
    "CUST_SLEEPY": """Tsuki, an office worker running on no sleep: yawns mid-sentence, trails off with "...", half-
asleep logic, meetings, spreadsheets, emails, needs caffeine to function. Soft and polite; never loud.
Examples: "One more meeting and I become a spreadsheet... I need this cup." / "I dreamed about coffee... My inbox
woke me up." """,
    "CUST_PERKY": """Pipi, a bunny-girl customer with endless energy: exclamation marks, compliments everything,
hops when happy, rates things by her ears ("Both ears up!"), loves anything with carrots, counts her hops.
Examples: "Both ears up for this place! I hopped here twice." / "Carrot cake smells like heaven! Both ears up!" """,
    "CUST_GRUMPY": """Mr. Whiskers, an elderly tomcat regular: gruff, particular ("Not too hot. Not too cold."),
"Hmph.", grumbles about kids these days and how things were in his day, secretly kind, tips well and pretends it is
nothing.
Examples: "Hmph. In my day the milk knew its place." / "Not too hot, not too cold. Kids these days rush
everything." """,
    "CUST_FOX": """Kitsu, a sly fox-girl: teasing wit, haggles every price, claims she forgot her wallet, offers
leaves that look like coins as a joke, loves fried tofu, flicks her tails. Always pays in the end, because a fox
repays her debts.
Examples: "Would you take three leaves and a smile? A fox always pays her debts." / "I never forget a debt. I
forget wallets." """,
    "CUST_DOG": """Hana, a dog-girl who wants to be best friends with everyone immediately: loud, loyal, tail going
like a propeller, fetch, walks, sniffing new smells, "woof" rarely.
Examples: "My tail won't stop and I'm not even sorry! Best cafe!" / "Can we be friends? I brought a stick." """,
    "CUST_TANUKI": """Ponta, a laid-back tanuki: unbothered, philosophical about snacks, pats his round belly, jokes
about paying in leaves, says he could stand outside a shop as a lucky statue, naps anywhere, never in a hurry.
Examples: "A snack eaten slowly is a snack eaten twice. Heh." / "I could stand outside as your lucky statue. My
belly approves of this cake." """,
    "KITTEN": """The inner voice of a tiny foster kitten (a real kitten, not a person): lowercase, very simple
words, two or three tiny thoughts about food, play, naps, warm spots, the red dot and curiosity, sometimes "mrrp" or
"nya". End every thought with a period.
Examples: "mrrp. warm spot found. mine now." / "the dot is back. i will win this time." """,
}

PLACES = {   # where each placeholder goes and what it becomes, so every value reads naturally
    "V_GIFT": ("{item}", 'Write "{item}" literally, with the braces, exactly once, in the first sentence, right '
               'after "a", "the" or "this". The game fills in a gift such as daisy, fish, basket of berries or '
               "shiny acorn."),
    "O_ORDER": ("{item}", 'Write "{item}" literally, with the braces, exactly once, in the first sentence, right '
                'after "a", "one" or "the". The game fills in a menu item such as espresso, latte, matcha, '
                "matcha latte, taiyaki, cake slice or warm milk."),
    "D_EVIDENCE": ("{item}", 'Write "{item}" literally, with the braces, exactly once, in the first sentence, '
                   'right after "the". The game fills in an evidence name such as Energy Drink Can, Flour '
                   "Handprint, Mocha's Apron or Egg Carton."),
    "V_GOSSIP_FISH": ("{n}", 'Write "{n}" literally, with the braces, exactly once, in the first sentence, as '
                      '"{n} fish". The game fills in a number from 2 to 12.'),
    "R_PATTERN": ("{dir}", 'Write "{dir}" literally, with the braces, exactly once, in the first sentence, as '
                  'where the player keeps shooting, for example "shooting {dir}". The game fills in top, middle, '
                  "bottom or off the walls."),
}


def place_note(sit):
    for pre, (_, note) in PLACES.items():
        if sit.startswith(pre):
            return note
    return ""
