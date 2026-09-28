"""The teacher's writing guide for the second generation of lines (tools/distill_gen2.py).

The judges and labellers keep describing each speaker with VOICES from distill_spec.py, so ratings stay comparable
with every earlier round; this file only tells the writer more. Two goals, both from the quality panel's complaints
about the first generation (build/quality/r4, r5): lines must sound like their speaker (no sentence anyone could
say), and every line must recombine: the game joins one line's first sentence to another line's later sentences,
so a later sentence may never lean on something only its own first sentence named.

Voice notes come from Tyler's character cards (Nia, Mako, Shio), the first-generation descriptions, and published
archetype and folklore sources, checked 2026-09-27: Wikipedia "Tsundere" and "Kuudere" (warmth leaks, then is
denied; a kuudere shows affection in actions, not words), Wikipedia "Kitsune" (tricksters who repay their debts,
fried tofu, many tails), Wikipedia "Japanese raccoon dog" and the Tanuki statue article (leaves turned into money,
lucky statues outside shops, round belly). Tanuki folklore's sake is left out: the game is all ages."""
from distill_spec import SITS, VOICES  # noqa: F401  (re-exported for the generator)

RULES = """Rules for every line:
- Wholesome and safe for all ages. No romance, flirting, alcohol or crude content.
- Plain ASCII only: no emoji, no curly quotes, no special symbols except ... ~ ! ? - , . ' and digits.
- One spoken line of two or three short sentences, 30 to 84 characters in all. No line breaks, stage directions,
  asterisks or surrounding quotes.
- The first sentence reacts to the situation. Each later sentence must still make sense if it followed a different
  first sentence this speaker might say here: never point back at something only the first sentence names (no
  "it", "that", "this", "he", "she", "they", "there", "too", "also" or "again" referring to it).
- Every sentence sounds like this speaker: their interests, habits and word choices. Never write a sentence anyone
  could say, like "Thank you.", "See you later!", "Great job." or "Keep it up."
- Use a signature phrase or verbal tic in at most one line in five. Vary openings, rhythm and length; never repeat
  a line or a near-copy."""

WRITER = {
    "NIA": """Talks like a tired indie developer: builds, bugs, patch notes, frame rates, "ship it", "cursed",
"I'm cooked". Short declaratives; an exclamation mark only when ironic. Cares through logistics: food, sleep, water,
a charger. Deflects thanks, braces for criticism of her builds, gets oddly specific about unimportant details when
something real comes up. Never gushes, never cheers, never says "yay".
Examples: "Build is cursed again. Don't touch the red terminal." / "Eat before you crash. The fridge has
dumplings." / "You found the exit bug. I owe you a snack." """,
    "MAKO": """Everything is a metric: scores, days, percentages, rankings, and she is always keeping count.
Declares fast and with too much conviction. Warmth leaks out by accident and she denies it in the next breath. When
she loses she rebuilds her pride on the spot ("I let you. For data."). Weaponized cute that never works on the
player. Never admits a loss plainly and never apologizes without covering it.
Examples: "You lasted longer than I thought. I had you at four days." / "That's not - can you just accept the thing
and not analyze it?" / "Nice shot. Not that I was watching. I was watching." """,
    "SHIO": """Clinical and formal: evidence, measurements, probabilities, lab notes. "Anyway." ends a topic; "Note
taken." acknowledges. Perfect grammar when annoyed. Affection shows in what she does (keeps the gift on her desk,
saves you a seat, re-runs a test for you), never in gushing or exclamation. Only ordinary social moments fluster
her: "Nya- that's- I wasn't-". Never says "I love it!" and never raises her voice.
Examples: "I wasn't waiting. I was assessing variables." / "The residue is sugar, not flour. Anyway." / "I saved you
the chair by the window. Efficiency is a form of courtesy." """,
    "MOCHA": """Runs the counter like a relay race: handoffs, tags, the next leg, table numbers, tickets. Warm,
brisk and gently bossy. Bribes everyone with pastries. Makes coffee puns and is visibly annoyed that she loves them.
Cheerful about being tired ("My feet filed a complaint. Denied."). Calls the player "new hire" in some lines, not
all. Says "nya" only when a rush goes perfectly.
Examples: "Tickets up, new hire. You take pastries, I take the machine." / "My feet filed a complaint. Denied,
we have a rush." / "Table four is thrilled. Have a croissant, you earned the flaky one." """,
    "KURO": """An old fisherman who speaks slowly in homemade proverbs about fish, tides, bait, nets, weather and
patience, then chuckles at them ("Heh."). Mentions his boat, the pond, the morning mist, the heron that steals his
catch. Quietly fond of the player and says so in understatement. Never hurried, never slangy, rarely exclaims.
Examples: "A calm line catches the hungry fish. Heh." / "The heron stole my breakfast again. Sit, the tide is slow
today." / "Sit a while. The tide doesn't need us yet." """,
    "SUZU": """Talks fast, loves flowers and news in equal measure: snapdragons, marigolds, sweet peas,
hydrangeas, what is blooming and who said what. Cannot keep a secret ("Don't tell anyone, but..."). Gossip is always
kind. "Oh my whiskers" in at most one line in five.
Examples: "The sweet peas came up overnight! Don't tell anyone, Kuro hums while he fishes." / "Hydrangeas are blue
this year! Mocha is testing a new muffin, did you hear?" / "Stand still, you have a petal on your ear." """,
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
"nya".
Examples: "mrrp. warm spot found. mine now." / "the dot is back. i will win this time." """,
}

PLACES = {   # where each placeholder goes and what it becomes, so every value reads naturally
    "V_GIFT": ("{item}", 'Write {item} exactly once, in the first sentence, always right after "a", "the" or '
               '"this". It becomes a gift such as: daisy, fish, basket of berries, shiny acorn.'),
    "O_ORDER": ("{item}", 'Write {item} exactly once, in the first sentence, right after "a", "one" or "the". '
                "It becomes a menu item such as: espresso, latte, matcha, matcha latte, taiyaki, cake slice, "
                "warm milk."),
    "D_EVIDENCE": ("{item}", 'Write {item} exactly once, in the first sentence, right after "the". It becomes '
                   "an evidence name such as: Energy Drink Can, Flour Handprint, Mocha's Apron, Egg Carton."),
    "V_GOSSIP_FISH": ("{n}", 'Write {n} exactly once, in the first sentence, as "{n} fish". It becomes a number '
                      "from 2 to 12."),
    "R_PATTERN": ("{dir}", "Write {dir} exactly once, in the first sentence, as where the player keeps shooting, "
                  'for example "shooting {dir}". It becomes: top, middle, bottom or off the walls.'),
}


def place_note(sit):
    for pre, (_, note) in PLACES.items():
        if sit.startswith(pre):
            return note
    return ""
