"""Build 06 (the dating sim): routes for Nia, Mako and Shio, taken from their source cards.

Tyler's decisions (2026-09-28): routes for these three, outfits as rewards (sexy but not explicit), the tells (ears,
tail, positioning) animated; suggestive dialogue is fine, and a moment that would go further fades to black. Nothing
explicit on screen. Everything structural comes from each card: the stats are the card's hidden stats, the
milestones are the card's milestones (made PG), every choice option is one of the card's progress or regression
triggers, and the stat changes are the card's own numbers. The writer (tools/date_gen.py) only writes the words and
the tell cues; tools/mkdate.py bakes the result into src/gen/date_data.c."""

EVENINGS = 14

ROLE = {
    "NIA": "your roommate since her own apartment flooded two weeks ago; an indie game developer who hosts her "
           "builds for you to playtest. She lives in your place and has quietly expanded into every room.",
    "MAKO": "your next-door neighbor, who lost a bet to you and has to be nice to you every day until day 30. She "
            "treats it as a contest she is losing, and it is slowly stopping being about the bet.",
    "SHIO": "your next-door neighbor, a forensic analyst whose water was shut off (administratively) because a court "
            "filing ate the week she should have paid the bill, so she knocks to borrow your bathroom.",
}

# the card's hidden stats: name -> starting value (0..100)
STATS = {
    "NIA": {"seen": 10, "debt": 30, "burnout": 40},
    "MAKO": {"nice": 40, "excuse": 80, "real": 5},
    "SHIO": {"proximity": 31, "trust": 18, "competence": 88, "case_bleed": 40},
}
PRIMARY = {"NIA": "seen", "MAKO": "real", "SHIO": "trust"}

# the card's progress and regression triggers, with the card's numbers
TRIGGERS = {
    "NIA": {
        "help_invisibly": ("help with something without making it visible or mentioning it", {"seen": 4, "debt": -3}),
        "notice_quietly": ("notice something wrong without asking her to perform or explain", {"seen": 6}),
        "stay_past_reason": ("stay after any reason to be in the room has run out", {"seen": 8, "burnout": -3}),
        "let_her_handle": ("do not comment when she handles something herself", {"seen": 3}),
        "give_space": ("give her space without making it mean anything", {"seen": 3, "debt": -2}),
        "pressure_explain": ("push her to explain herself or say what she needs", {"seen": -6, "debt": 8}),
        "public_help": ("help her in a way that highlights the help", {"seen": -5, "debt": 10}),
        "too_much_warmth": ("pile on attention or warmth all at once", {"seen": -3, "debt": 6}),
        "call_her_tired": ("point out she looks tired before she names it", {"seen": -4, "debt": 4}),
        "thank_the_invisible": ("thank her out loud for something she arranged to be invisible", {"seen": -4, "debt": 6}),
    },
    "MAKO": {
        "treat_bet_real": ("treat the bet as real, without mocking it", {"real": 5, "nice": 5}),
        "let_excuse_stand": ("let her thin excuse stand without challenging it", {"excuse": -15, "real": 4}),
        "ignore_the_slip": ("not mention it when she slips and is genuinely nice", {"real": 6, "excuse": -8}),
        "stay_after_slip": ("stay after she slips instead of leaving or teasing", {"real": 10, "nice": 5}),
        "show_up_after_rude": ("show up for her after she has been rude", {"real": 8, "nice": 5}),
        "compete_back": ("meet her competitiveness with some of your own, playfully", {"nice": 4, "real": 2}),
        "mock_the_bet": ("mention the bet ironically", {"real": -10, "nice": -10}),
        "laugh_at_her": ("laugh at her genuinely, not with her", {"real": -12, "nice": -8}),
        "use_sincerity": ("use something sincere she said against her", {"real": -15}),
        "charity_patience": ("be patient with her in a way that feels like charity", {"real": -4}),
    },
    "SHIO": {
        "stay_no_push": ("stay without pushing", {"proximity": 3, "trust": 2}),
        "notice_ears_quietly": ("notice her ears without reacting to them", {"proximity": 5}),
        "remember_detail": ("remember a small detail about her", {"trust": 4}),
        "calm_case_talk": ("receive a mention of her cases calmly", {"trust": 6, "case_bleed": -4}),
        "not_flinch": ("not flinch at the grim parts of her work", {"trust": 4}),
        "stay_after_errand": ("stay after her original reason for being here ended", {"trust": 3, "proximity": 3}),
        "make_her_laugh": ("draw a first genuine laugh out of her", {"competence": -5, "trust": 2}),
        "push_early": ("push closer before she signals it is welcome", {"proximity": -4, "trust": -3}),
        "pity": ("show her pity", {"trust": -10}),
        "react_to_ears": ("react out loud to her ears", {"trust": -4, "proximity": -3}),
        "call_it_morbid": ("dismiss her work as morbid", {"trust": -10}),
        "treat_fragile": ("treat her as fragile after she has shown she isn't", {"trust": -8}),
    },
}

# (id, kind, threshold on the primary stat, what happens - PG, from the card's milestones, outfit unlocked)
BEATS = {
    "NIA": [
        ("intro", "intro", 0, "You come home. Nia glances up from her laptop: she made too much ramen again, if you want "
         "some, or you can ignore her, that's fine too. First real evening as roommates.", 0),
        ("m1", "milestone", 15, "Her door is open with no task attached. She is just there, near where you are, and "
         "does not give a reason.", 0),
        ("m2", "milestone", 28, "She answers something she didn't have to: a real answer about her last roommate or "
         "why the build matters, given plainly and then dropped.", 0),
        ("m3", "milestone", 40, "Her tail gives her away (tip curled, pleased) and she does not cover it. She lets you "
         "see it.", 1),
        ("m4", "milestone", 52, "She makes something for you (tea, ramen, a tiny level) without framing it as logistics.", 0),
        ("m5", "milestone", 64, "She admits she noticed: the small things you did, the times you stayed.", 0),
        ("m6", "milestone", 76, "The work is done and she stays anyway. No manufactured reason. She sits with you on the "
         "couch, shoulder to shoulder, and lets it be about you. It gets quiet and close; a first kiss if you earn it "
         "in the scene.", 2),
        ("h1", "hangout", 0, "A late playtest session: she wants your honest read on a level.", 0),
        ("h2", "hangout", 0, "The build breaks at midnight and she is quietly panicking behind flat sentences.", 0),
        ("h3", "hangout", 0, "A walk to the 24-hour convenience store for flat energy drinks and snacks.", 0),
        ("h4", "hangout", 0, "She has migrated to the kitchen table with her laptop while you cook.", 0),
        ("h5", "hangout", 0, "Laundry night: she folds with machine precision, your things included, and does not mention it.", 0),
        ("h6", "hangout", 0, "A power cut: the building goes dark and quiet, her laptop is at nine percent, and there is nothing to do but sit on the couch and talk.", 0),
        ("end_best", "ending", 76, "Evening 14. She tells you, in her own precise way, that she wants to stay after the "
         "flood is fixed, not as a roommate who earns her keep. Romantic and tender, and a little suggestive "
         "in her dry, precise way: a kiss, and if the moment goes further, fade to black.", 0),
        ("end_friends", "ending", 40, "Evening 14. The flood repairs are done. She is moving back, but she leaves her "
         "door code with you and a save file named after you. Warm friendship, a door left open.", 0),
        ("end_drift", "ending", 0, "Evening 14. Her apartment is fixed. She thanks you politely, packs efficiently and "
         "goes. Bittersweet and quiet.", 0),
    ],
    "MAKO": [
        ("intro", "intro", 0, "Three sharp knocks, one softer. Mako is at your door holding food: she made extra, and "
         "she is only here because she lost a bet, as she explains with far too much conviction.", 0),
        ("m1", "milestone", 10, "The Extra: she made too much again. The reason is thin. She knocks anyway.", 0),
        ("m2", "milestone", 22, "The Slip: for thirty seconds she is simply nice, with no performance. Neither of you "
         "mentions it.", 0),
        ("m3", "milestone", 34, "Her friend Yui drops by and watches the two of you for four minutes without saying "
         "anything. Mako knows exactly what Yui is thinking.", 1),
        ("m4", "milestone", 46, "The Schedule: she has been timing things around yours. You notice. She notices that "
         "you noticed.", 0),
        ("m5", "milestone", 58, "Day 29: she is quieter, still compliant, and calculating something.", 0),
        ("m6", "milestone", 70, "Day 30: 'Done.' The bet is over. She doesn't leave immediately. The rivalry turns "
         "charged: she dares you, you dare her back.", 2),
        ("h1", "hangout", 0, "A rematch at paw hockey on the old console; she keeps score out loud.", 0),
        ("h2", "hangout", 0, "She critiques your apartment, your posture and your taste in snacks, all correct.", 0),
        ("h3", "hangout", 0, "She shows up with a barely valid reason: she needs to borrow a ladder she does not need.", 0),
        ("h4", "hangout", 0, "She is being extremely nice to you on purpose, with a visible score.", 0),
        ("h5", "hangout", 0, "The stairwell: she is carrying a box that is clearly too heavy and will not admit it.", 0),
        ("h6", "hangout", 0, "A rainy evening: she brings a board game she is sure she will win, and explains the rules in a way that favours her.", 0),
        ("end_best", "ending", 70, "Day 31. She knocks with no reason. 'Hi.' She waits. Romantic and flirtatious: she stays, "
         "kisses you, then dares you to say something about it; teasing and suggestive; if it goes further, fade to "
         "black.", 0),
        ("end_friends", "ending", 34, "Day 31. She knocks to demand a rematch, and you both know it is not about the "
         "rematch. Rivals, friends, something warm and unsaid.", 0),
        ("end_drift", "ending", 0, "Day 31. The bet is over and she goes back to being professionally pleasant in the "
         "hallway. The headband is straight again.", 0),
    ],
    "SHIO": [
        ("intro", "intro", 0, "One knock. Shio is in the hallway in her cardigan, glasses fogged, ears flat: her water "
         "was shut off, administratively, and she just needs your bathroom. She won't be long.", 0),
        ("m1", "milestone", 24, "She stays past the errand without a reason, and the conversation runs long.", 0),
        ("m2", "milestone", 34, "She mentions the weight of a case, carefully and without detail (all-ages), then "
         "watches how you take it.", 0),
        ("m3", "milestone", 44, "Her ears move toward you and she does not explain it.", 1),
        ("m4", "milestone", 54, "She dozes off on your couch over her case notes, somewhere that isn't her apartment.", 0),
        ("m5", "milestone", 64, "She says what she means without the wrapper of data and variables.", 0),
        ("m6", "milestone", 74, "She asks you not to go. Quiet and charged: she is direct about wanting you close, "
         "in her clinical way.", 2),
        ("h1", "hangout", 0, "She sends you a forensic fact at two in the morning and then appears at your door.", 0),
        ("h2", "hangout", 0, "She quietly fixes something in your apartment and pretends she didn't.", 0),
        ("h3", "hangout", 0, "She made too much food. Inefficient to waste it.", 0),
        ("h4", "hangout", 0, "A late-night stairwell conversation after court prep.", 0),
        ("h5", "hangout", 0, "She needs a second pair of eyes on a crossword that turns out to be about poisons, and is quietly pleased when you get one.", 0),
        ("h6", "hangout", 0, "A thunderstorm: she knocks with a flashlight and a very rational reason to wait it out at your place.", 0),
        ("end_best", "ending", 74, "Evening 14. The water is back on and she still knocks. She says she trusts you, out "
         "loud, and her ears stay turned toward you. Romantic and gently suggestive in her "
         "precise way: a kiss; if the moment goes further, fade to black.", 0),
        ("end_friends", "ending", 44, "Evening 14. The water works again. She leaves something at your door: not a "
         "conversation, just a thing, and a note with a forensic fact about you.", 0),
        ("end_drift", "ending", 0, "Evening 14. The water is back. She thanks you formally for the access and the "
         "hallway goes quiet.", 0),
    ],
}

LOCATIONS = ["doorway", "kitchen", "couch", "hallway", "stairwell", "store", "her_room"]
FACES = ["neutral", "happy", "annoyed", "surprised", "smug", "flustered", "sad", "sparkle", "tired"]
EARS = ["up", "perk", "flat", "back", "curl", "droop", "still", "swivel"]
TAILS = ["sway", "flick", "still", "curl", "tuck", "wrap", "low", "wag", "puff"]
POSES = ["mid", "far", "close", "lean", "away"]
GESTURES = {"NIA": ["none", "fringe"], "MAKO": ["none", "chain"], "SHIO": ["none", "glasses", "wrist"]}

WRITER_RULES = """Rules:
- This is a dating sim with adult characters. Warmth, teasing, flirting, innuendo, suggestive dialogue and romantic
  tension are welcome, and so is closeness shown plainly (a kiss, a hand, leaning in, sitting close). Nothing explicit
  on screen: no sexual acts, no nudity, no sexual description of bodies. When a moment would go further than that,
  cut away with a fade to black: a line {"who": "fade", "text": "Later."} whose text (at most 30 characters) is shown
  over a black screen, like "Later." or "The lamp clicks off."; the scene may continue after it or end there. At most
  one fade per scene. All characters are adults; never describe them as young or small.
- How suggestive a scene is follows the story so far and the card's pacing: the first evenings are guarded and light,
  the later milestones and the best ending can be openly flirtatious and charged. She stays herself when she flirts:
  the card's voice and tells, not a generic romance heroine.
- Plain ASCII only. Each line is at most 140 characters (a dialogue box holds three short lines). No stage directions inside spoken lines; a
  narrator line (who "narrator") may describe a tell in a short sentence ("Her tail goes completely still.").
- The character sounds exactly like her source card: its speech patterns, emotional leakage and quote bank, in
  fresh words; echo at most one signature line from the card's quote bank per scene.
- Show her feelings through the cues on every line of hers: face, ears, tail, pose, gesture, blush, look_away.
  The card says which tells mean what (ears and tail are the honest version of her face). Use them deliberately:
  a tail that goes completely still, ears that flatten or swivel, stepping closer or back.
- "you" lines are the player's own words, short and natural, written as the player speaks.
- Choices: exactly the number asked for, each with 3 or 4 options. Each option is one short thing the player says
  or does (at most 70 characters) and names exactly one trigger id from the list. Each choice offers at least one
  progress trigger and at least one regression trigger, and the options must not reveal which is which.
- After each option, 1 to 3 reaction lines (hers, or a narrator line about a tell) that react to that option.
- The player's pick changes only its reaction lines, so any lines between two choices must make sense whichever
  option came before them. The scene ends with the final choice's reactions: put no steps after the last choice.
Reply with JSON only."""

SCHEMA_HINT = """{"location": one of LOCATIONS, "time": "evening" or "late",
 "steps": [
   {"line": {"who": "her" | "you" | "narrator" | "fade", "text": "...", "face": FACE, "ears": EARS, "tail": TAIL,
             "pose": POSE, "gesture": GESTURE, "blush": true|false, "look_away": true|false}},
   {"choice": {"prompt": "short question to the player", "options": [
       {"text": "...", "trigger": TRIGGER_ID, "reaction": [ line objects like above ]}, ... ]}},
   ...
 ]}
(Cue fields are only needed on her lines; "you", "narrator" and "fade" lines need only who and text.)"""
