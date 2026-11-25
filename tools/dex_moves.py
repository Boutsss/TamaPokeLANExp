# -*- coding: utf-8 -*-
"""The move list, hand-authored. Source of truth for moves.h via gen_moves.py.

86 moves, trimmed from Gen 1's 165 and then topped up with the cheap early
attacks the early game needs: 63 attacking (three or four per type, a
cheap one and a strong one), 11 stat-stage moves and 2 heals. STRUGGLE at the
end makes 77 -- it is a fallback, not a learnable move.

Not a pure Gen 1 list, and it can't be. dex_types.py gives the 151 their
CURRENT typings, so the dex contains Fairy (Clefable, Mr. Mime), Steel
(Magneton) and Dark types -- and Fairy moves are Gen 6, Steel and Dark moves
are Gen 2. Restricting to Red/Blue's move list would leave exactly those
species with no same-type move to their name. Entries marked LATER below are
in for that reason: they buy STAB for a typing the game already committed to.
Ghost is the same story -- Gen 1 Ghost had no usable attacking move at all,
hence SHADOW BALL.

Display names are capped at 12 characters: four move buttons have to fit
across a 466 px round panel at text size 2. ANCIENTPOWER is the games' own
Gen 3 spelling; DAZZLE GLEAM is ours.

No PP by design. Status conditions (burn/para/sleep/poison) are deliberately
absent for now -- the effect enum has room and they land in a later phase.
"""

# Move categories. There is no separate special-attack IV: special moves run
# off ivAtk/trAtk against the species' bSpA, and special defence off
# ivDef/trDef against bSpD. See gen_moves.py and the note in dex_stats.py.
MC_PHYS, MC_SPEC, MC_STATUS = 0, 1, 2

# Status ailments. Battle-only by design: they live in the battle state and are
# cleared when it ends, never written to the pet or replayed by the RTC's
# offline catch-up, so a burn can never grind against the care sim.
#
# There is no dedicated status move for these (no THUNDER WAVE, no SLEEP
# POWDER), so they ride along as a SECONDARY chance on a damaging move -- which
# is why they need their own two fields rather than reusing `effect`, whose one
# slot is already spent on things like EF_RECOIL.
AIL_NONE = 0
AIL_PARA = 1        # speed cut, and some turns are lost outright
AIL_BURN = 2        # chip damage each turn, physical attack cut
AIL_POISON = 3      # chip damage each turn
AIL_SLEEP = 4       # loses its turn for a few turns (SLEEP POWDER, HYPNOSIS)
AIL_FREEZE = 5      # loses its turn until it thaws
AIL_CONFUSE = 6     # may hit itself instead

# Effects. Anything not listed here is a plain damaging move.
EF_NONE = 0
EF_STAGE = 1        # raise/lower stat stages (statMask + stages + target)
EF_RECOIL = 2       # param = denominator: 3 -> user takes 1/3 of damage dealt
EF_DRAIN = 3        # param = percent of damage dealt healed back
EF_FIXED_LVL = 4    # damage = user's level, ignores every stat
EF_FIXED = 5        # param = flat damage
EF_PRIORITY = 6     # param = priority bracket, moves before slower brackets
EF_NEVER_MISS = 7   # skips the accuracy roll entirely
EF_MULTI = 8        # hits 2-5 times, power is per hit
EF_HEAL = 9         # param = percent of max VIT restored
EF_RECHARGE = 10    # user loses the following turn, exposed
EF_CHARGE = 11      # turn 1 charges; param 1 = invulnerable while charging
# --- v3.37: the rules the second batch of moves needs (battle.cpp) ---------
# APPEND-ONLY like the moves: moves.h stores the number, and both the
# generator and the engine name them.
EF_PROTECT = 12       # nothing hits the user this turn; chained use gets less likely
EF_ENDURE = 13        # survives this turn's hits at 1 HP
EF_WEATHER = 14       # param W_*: 1 rain, 2 sun, 3 sand, 4 hail, five turns
EF_HAZARD = 15        # param 1 stealth rock, 2 spikes, 3 toxic spikes, 4 sticky web
EF_SPIN = 16          # damage, then clears the user's side of hazards, seeds and binds
EF_DEFOG = 17         # clears hazards on both sides and the foe's screens; foe evasion -1
EF_SCREEN = 18        # param 1 reflect, 2 light screen, 3 aurora veil (hail only)
EF_SUBSTITUTE = 19    # a quarter of max HP into a decoy that takes the hits
EF_REST = 20          # full heal, sleeps exactly two turns
EF_SLEEPTALK = 21     # asleep: one of its other moves, at random
EF_SNORE = 22         # damage, only while asleep
EF_HALFHP = 23        # damage = half the target's current HP
EF_ENDEAVOR = 24      # the target's HP drops to the user's
EF_OHKO = 25          # faints the target; fails on a higher level
EF_SELFKO = 26        # the user faints (EXPLOSION, SELF-DESTRUCT)
EF_VARPOW = 27        # param VP_*: power decided at use time
EF_COUNTER = 28       # param 1 physical x2, 2 special x2, 3 either x1.5 (last hit taken)
EF_FAKEOUT = 29       # first turn out only; param 0 also flinches
EF_LEECHSEED = 30     # 1/8 a turn drained to the opposing creature
EF_BIND = 31          # damage, then 1/8 a turn for 4-5 turns, and it cannot switch
EF_YAWN = 32          # falls asleep at the end of next turn
EF_TAUNT = 33         # cannot use status moves for three turns
EF_ENCORE = 34        # repeats its last move for three turns
EF_CURSE = 35         # Ghost: half its HP to curse the foe; others +ATK +DEF -SPE
EF_BELLYDRUM = 36     # half its HP for ATK +6
EF_SHELLSMASH = 37    # ATK/SP.A/SPE +2, DEF/SP.D -1
EF_HAZE = 38          # every stage on both sides back to 0
EF_CLEARSMOG = 39     # damage, and the target's stages back to 0
EF_PSYCHUP = 40       # copies the target's stages
EF_PAINSPLIT = 41     # both creatures' HP to the average
EF_TAILWIND = 42      # four turns of double speed for the user's side
EF_TRICKROOM = 43     # five turns: slower first
EF_SWAGGER = 44       # raises the target's statMask by stages, then confuses it
EF_DESTINYBOND = 45   # whoever faints the user next faints too
EF_PERISH = 46        # both creatures out now faint in three turns
EF_AQUARING = 47      # 1/16 a turn back; param 1 (INGRAIN) also cannot switch
EF_WISH = 48          # half its max HP back at the end of next turn
EF_SAFEGUARD = 49     # five turns: no status for the user's side
EF_MIST = 50          # five turns: no stat drops for the user's side
EF_FOCUSENERGY = 51   # param 2 = crit stage +2 (FOCUS ENERGY), 3 = next hit crits (LASER FOCUS)
EF_SPLASH = 52        # nothing at all
EF_ACUPRESSURE = 53   # one random stat +2
EF_TOPSYTURVY = 54    # the target's stages inverted
EF_CHARGEUP = 55      # next Electric move doubled, SP.D +1
EF_HEAL_WEATHER = 56  # 1/2, 2/3 in sun (or sand, param 1), 1/4 in other weather
EF_TRAP = 57          # the target cannot switch out
EF_TERRAIN = 58       # param 1 electric, 2 grassy, 3 misty, 4 psychic, five turns
EF_MEMENTO = 59       # the user faints; statMask/stages on the target
EF_HIDDENPOWER = 60   # type from the IVs, power 60
EF_MAGNITUDE = 61     # power 10-150 by the games' odds
EF_KNOCKOFF = 62      # x1.5 into a held item, which it then loses for the battle
EF_FINALGAMBIT = 63   # damage = the user's HP, and the user faints
EF_FOCUSPUNCH = 64    # fails if the user was hit this turn before it moved
EF_SUCKER = 65        # fails unless the target is about to attack
EF_DREAMEATER = 66    # only on a sleeping target; param = percent drained
EF_TRIATTACK = 67     # ailChance of burn, freeze or paralysis, one of the three
EF_WAKEUP = 68        # double power on that ailment (param), then cures it
EF_ESCALATE = 69      # doubles each consecutive use, up to param doublings
EF_LOCKIN = 70        # locked in for 2-3 turns, then confused
EF_FUTURE = 71        # lands two turns later

# EF_VARPOW kinds
VP = ('NONE FACADE RETURN FRUSTRATION PAYBACK ASSURANCE REVENGE HEX VENOSHOCK BRINE ACROBATICS '
      'GYRO ELECTRO REVERSAL ERUPTION STORED PUNISH WEATHERBALL WRINGOUT TANTRUM WEIGHT HEAVY').split()

# Flags, per move (dex_moves_meta.py). Bits in MoveEntry::flags.
MF_IGNORE_STAGES = 1   # ignores the target's DEF/SP.D stages (SACRED SWORD)
MF_PHYS_DEF = 2        # special, but against Defense (PSYSHOCK)
MF_FOUL = 4            # uses the TARGET's attack (FOUL PLAY)
MF_BREAK_SCREEN = 8    # breaks REFLECT and LIGHT SCREEN first (BRICK BREAK)
MF_FREEZE_DRY = 16     # super effective on Water
MF_HITS_AIR = 32       # reaches a creature in the air (FLY, BOUNCE)
MF_HITS_GROUND = 64    # reaches one underground (DIG)
MF_HITS_WATER = 128    # reaches one underwater (DIVE)
MF_RAIN_SURE = 256     # cannot miss in rain (THUNDER)
MF_HAIL_SURE = 512     # cannot miss in hail (BLIZZARD)

# Stat bitmask for EF_STAGE. One delta applies to every bit set, which is how
# DRAGON DANCE (ATK+SPE) and BULK UP (ATK+DEF) get to be one table row.
ST_ATK, ST_DEF, ST_SPA, ST_SPD, ST_SPE = 1, 2, 4, 8, 16
# v3.37: accuracy and evasion stages, for SAND ATTACK, DOUBLE TEAM and friends
ST_ACC, ST_EVA = 32, 64

TG_SELF, TG_FOE = 0, 1

# name (<=12 chars), pokeapi slug, type, category, power, accuracy,
# effect, param, statMask, stages, target
#
# accuracy 0 means the move cannot miss (only ever paired with EF_NEVER_MISS
# or a status move, which never rolls).
MOVES = [
    # --- NORMAL -----------------------------------------------------------
    # The cheap early attacks. Without these a young creature had NOTHING of its
    # own: about 15% of species reached level 15 with no attacking move at all,
    # because their real early moves were missing here and they were quietly
    # leaning on TMs to fill the gap. That only became visible once TMs were
    # gated -- a level 1 Squirtle had been opening with SURF instead.
    ("TACKLE",       "tackle",        'normal',   MC_PHYS,  40, 100, EF_NONE,      0, 0, 0, TG_FOE),
    ("SCRATCH",      "scratch",       'normal',   MC_PHYS,  40, 100, EF_NONE,      0, 0, 0, TG_FOE),
    ("POUND",        "pound",         'normal',   MC_PHYS,  40, 100, EF_NONE,      0, 0, 0, TG_FOE),
    ("FURY ATTACK",  "fury-attack",   'normal',   MC_PHYS,  15,  85, EF_MULTI,     0, 0, 0, TG_FOE),
    ("QUICK ATTACK", "quick-attack",  'normal',   MC_PHYS,  40, 100, EF_PRIORITY,  1, 0, 0, TG_FOE),
    ("SWIFT",        "swift",         'normal',   MC_SPEC,  60,   0, EF_NEVER_MISS, 0, 0, 0, TG_FOE),
    ("BODY SLAM",    "body-slam",     'normal',   MC_PHYS,  85, 100, EF_NONE,      0, 0, 0, TG_FOE),
    ("DOUBLE-EDGE",  "double-edge",   'normal',   MC_PHYS, 120, 100, EF_RECOIL,    3, 0, 0, TG_FOE),
    ("HYPER BEAM",   "hyper-beam",    'normal',   MC_SPEC, 150,  90, EF_RECHARGE,  0, 0, 0, TG_FOE),
    # --- FLYING (the early ones; WING ATTACK and up are further down) ------
    ("PECK",         "peck",          'flying',   MC_PHYS,  35, 100, EF_NONE,      0, 0, 0, TG_FOE),
    # --- FIRE -------------------------------------------------------------
    ("EMBER",        "ember",         'fire',     MC_SPEC,  40, 100, EF_NONE,      0, 0, 0, TG_FOE, AIL_BURN, 10),
    ("FIRE PUNCH",   "fire-punch",    'fire',     MC_PHYS,  75, 100, EF_NONE,      0, 0, 0, TG_FOE, AIL_BURN, 10),
    ("FLAMETHROWER", "flamethrower",  'fire',     MC_SPEC,  90, 100, EF_NONE,      0, 0, 0, TG_FOE, AIL_BURN, 10),
    ("FIRE BLAST",   "fire-blast",    'fire',     MC_SPEC, 110,  85, EF_NONE,      0, 0, 0, TG_FOE, AIL_BURN, 10),
    # --- WATER ------------------------------------------------------------
    ("BUBBLE",       "bubble",        'water',    MC_SPEC,  30, 100, EF_NONE,      0, 0, 0, TG_FOE),
    ("WATER GUN",    "water-gun",     'water',    MC_SPEC,  40, 100, EF_NONE,      0, 0, 0, TG_FOE),
    ("WATERFALL",    "waterfall",     'water',    MC_PHYS,  80, 100, EF_NONE,      0, 0, 0, TG_FOE),
    ("SURF",         "surf",          'water',    MC_SPEC,  90, 100, EF_NONE,      0, 0, 0, TG_FOE),
    ("HYDRO PUMP",   "hydro-pump",    'water',    MC_SPEC, 110,  80, EF_NONE,      0, 0, 0, TG_FOE),
    # --- ELECTRIC ---------------------------------------------------------
    ("SPARK",        "spark",         'electric', MC_PHYS,  35, 100, EF_NONE,      0, 0, 0, TG_FOE, AIL_PARA, 20),
    ("THUNDERSHOCK", "thunder-shock", 'electric', MC_SPEC,  40, 100, EF_NONE,      0, 0, 0, TG_FOE, AIL_PARA, 10),
    ("THUNDERPUNCH", "thunder-punch", 'electric', MC_PHYS,  75, 100, EF_NONE,      0, 0, 0, TG_FOE, AIL_PARA, 10),
    ("THUNDERBOLT",  "thunderbolt",   'electric', MC_SPEC,  90, 100, EF_NONE,      0, 0, 0, TG_FOE, AIL_PARA, 10),
    ("THUNDER",      "thunder",       'electric', MC_SPEC, 110,  70, EF_NONE,      0, 0, 0, TG_FOE, AIL_PARA, 30),
    # --- GRASS ------------------------------------------------------------
    ("ABSORB",       "absorb",        'grass',    MC_SPEC,  20, 100, EF_DRAIN,    50, 0, 0, TG_FOE),
    ("VINE WHIP",    "vine-whip",     'grass',    MC_PHYS,  45, 100, EF_NONE,      0, 0, 0, TG_FOE),
    ("RAZOR LEAF",   "razor-leaf",    'grass',    MC_PHYS,  55,  95, EF_NONE,      0, 0, 0, TG_FOE),
    ("MEGA DRAIN",   "mega-drain",    'grass',    MC_SPEC,  40, 100, EF_DRAIN,    50, 0, 0, TG_FOE),
    # charges in the open: the one 2-turn move that really does cost you a hit
    ("SOLAR BEAM",   "solar-beam",    'grass',    MC_SPEC, 120, 100, EF_CHARGE,    2, 0, 0, TG_FOE),
    # --- ICE --------------------------------------------------------------
    ("AURORA BEAM",  "aurora-beam",   'ice',      MC_SPEC,  65, 100, EF_NONE,      0, 0, 0, TG_FOE),
    ("ICE PUNCH",    "ice-punch",     'ice',      MC_PHYS,  75, 100, EF_NONE,      0, 0, 0, TG_FOE, AIL_FREEZE, 10),
    ("ICE BEAM",     "ice-beam",      'ice',      MC_SPEC,  90, 100, EF_NONE,      0, 0, 0, TG_FOE, AIL_FREEZE, 10),
    ("BLIZZARD",     "blizzard",      'ice',      MC_SPEC, 110,  70, EF_NONE,      0, 0, 0, TG_FOE, AIL_FREEZE, 10),
    # --- FIGHTING ---------------------------------------------------------
    ("KARATE CHOP",  "karate-chop",   'fighting', MC_PHYS,  50, 100, EF_NONE,      0, 0, 0, TG_FOE),
    ("SEISMIC TOSS", "seismic-toss",  'fighting', MC_PHYS,   0, 100, EF_FIXED_LVL, 0, 0, 0, TG_FOE),
    ("SUBMISSION",   "submission",    'fighting', MC_PHYS,  80,  80, EF_RECOIL,    4, 0, 0, TG_FOE),
    # 130 in the modern games; 100 here so one move can't decide a gym
    ("HI JUMP KICK", "high-jump-kick",'fighting', MC_PHYS, 100,  90, EF_NONE,      0, 0, 0, TG_FOE),
    # --- POISON -----------------------------------------------------------
    ("POISON STING", "poison-sting",  'poison',   MC_PHYS,  15, 100, EF_NONE,      0, 0, 0, TG_FOE, AIL_POISON, 30),
    ("ACID",         "acid",          'poison',   MC_SPEC,  40, 100, EF_NONE,      0, 0, 0, TG_FOE, AIL_POISON, 10),
    ("SLUDGE",       "sludge",        'poison',   MC_SPEC,  65, 100, EF_NONE,      0, 0, 0, TG_FOE, AIL_POISON, 30),
    ("SLUDGE BOMB",  "sludge-bomb",   'poison',   MC_SPEC,  90, 100, EF_NONE,      0, 0, 0, TG_FOE, AIL_POISON, 30),  # LATER (Gen 2)
    # --- GROUND -----------------------------------------------------------
    ("BONE CLUB",    "bone-club",     'ground',   MC_PHYS,  65,  85, EF_NONE,      0, 0, 0, TG_FOE),
    ("DIG",          "dig",           'ground',   MC_PHYS,  80, 100, EF_CHARGE,    1, 0, 0, TG_FOE),
    ("EARTHQUAKE",   "earthquake",    'ground',   MC_PHYS, 100, 100, EF_NONE,      0, 0, 0, TG_FOE),
    # --- FLYING -----------------------------------------------------------
    ("WING ATTACK",  "wing-attack",   'flying',   MC_PHYS,  60, 100, EF_NONE,      0, 0, 0, TG_FOE),
    ("DRILL PECK",   "drill-peck",    'flying',   MC_PHYS,  80, 100, EF_NONE,      0, 0, 0, TG_FOE),
    ("FLY",          "fly",           'flying',   MC_PHYS,  90,  95, EF_CHARGE,    1, 0, 0, TG_FOE),
    # --- PSYCHIC ----------------------------------------------------------
    ("PSYWAVE",      "psywave",       'psychic',  MC_SPEC,  30, 100, EF_NONE,      0, 0, 0, TG_FOE),
    ("CONFUSION",    "confusion",     'psychic',  MC_SPEC,  50, 100, EF_NONE,      0, 0, 0, TG_FOE, AIL_CONFUSE, 10),
    ("PSYBEAM",      "psybeam",       'psychic',  MC_SPEC,  65, 100, EF_NONE,      0, 0, 0, TG_FOE, AIL_CONFUSE, 10),
    ("PSYCHIC",      "psychic",       'psychic',  MC_SPEC,  90, 100, EF_NONE,      0, 0, 0, TG_FOE),
    # --- BUG --------------------------------------------------------------
    ("BUG BITE",     "bug-bite",      'bug',      MC_PHYS,  30, 100, EF_NONE,      0, 0, 0, TG_FOE),
    ("PIN MISSILE",  "pin-missile",   'bug',      MC_PHYS,  25,  95, EF_MULTI,     0, 0, 0, TG_FOE),
    # 20 power in Gen 1, 80 since Gen 7 -- the modern value, like the stats
    ("LEECH LIFE",   "leech-life",    'bug',      MC_PHYS,  80, 100, EF_DRAIN,    50, 0, 0, TG_FOE),
    ("MEGAHORN",     "megahorn",      'bug',      MC_PHYS, 120,  85, EF_NONE,      0, 0, 0, TG_FOE),  # LATER (Gen 2)
    # The three above reach almost no Gen 1 bug: BUTTERFREE and PINSIR learn
    # none of them, and would have gone to a gym with no same-type move at
    # all. These two are the standard modern Bug attacks and fix exactly that.
    ("BUG BUZZ",     "bug-buzz",      'bug',      MC_SPEC,  90, 100, EF_NONE,      0, 0, 0, TG_FOE),  # LATER (Gen 4)
    ("X-SCISSOR",    "x-scissor",     'bug',      MC_PHYS,  80, 100, EF_NONE,      0, 0, 0, TG_FOE),  # LATER (Gen 4)
    # --- ROCK -------------------------------------------------------------
    ("ROCK SMASH",   "rock-smash",    'fighting', MC_PHYS,  40, 100, EF_NONE,      0, 0, 0, TG_FOE),
    ("ROCK THROW",   "rock-throw",    'rock',     MC_PHYS,  50,  90, EF_NONE,      0, 0, 0, TG_FOE),
    ("ROCK SLIDE",   "rock-slide",    'rock',     MC_PHYS,  75,  90, EF_NONE,      0, 0, 0, TG_FOE),
    ("ANCIENTPOWER", "ancient-power", 'rock',     MC_SPEC,  60, 100, EF_NONE,      0, 0, 0, TG_FOE),  # LATER (Gen 2)
    # --- GHOST ------------------------------------------------------------
    ("LICK",         "lick",          'ghost',    MC_PHYS,  30, 100, EF_NONE,      0, 0, 0, TG_FOE, AIL_PARA, 30),
    ("NIGHT SHADE",  "night-shade",   'ghost',    MC_SPEC,   0, 100, EF_FIXED_LVL, 0, 0, 0, TG_FOE),
    ("SHADOW BALL",  "shadow-ball",   'ghost',    MC_SPEC,  80, 100, EF_NONE,      0, 0, 0, TG_FOE),  # LATER (Gen 2)
    # --- DRAGON -----------------------------------------------------------
    ("DRAGON RAGE",  "dragon-rage",   'dragon',   MC_SPEC,   0, 100, EF_FIXED,    40, 0, 0, TG_FOE),
    ("DRAGON CLAW",  "dragon-claw",   'dragon',   MC_PHYS,  80, 100, EF_NONE,      0, 0, 0, TG_FOE),  # LATER (Gen 3)
    ("OUTRAGE",      "outrage",       'dragon',   MC_PHYS, 120, 100, EF_LOCKIN,    0, 0, 0, TG_FOE),  # LATER (Gen 2)
    # --- DARK -------------------------------------------------------------
    # BITE existed in Gen 1 but as a Normal move; Dark from Gen 2 on
    ("BITE",         "bite",          'dark',     MC_PHYS,  60, 100, EF_NONE,      0, 0, 0, TG_FOE),  # LATER
    ("CRUNCH",       "crunch",        'dark',     MC_PHYS,  80, 100, EF_NONE,      0, 0, 0, TG_FOE),  # LATER (Gen 2)
    # --- STEEL ------------------------------------------------------------
    ("IRON HEAD",    "iron-head",     'steel',    MC_PHYS,  80, 100, EF_NONE,      0, 0, 0, TG_FOE),  # LATER (Gen 4)
    ("FLASH CANNON", "flash-cannon",  'steel',    MC_SPEC,  80, 100, EF_NONE,      0, 0, 0, TG_FOE),  # LATER (Gen 4)
    # --- FAIRY ------------------------------------------------------------
    ("DAZZLE GLEAM", "dazzling-gleam",'fairy',    MC_SPEC,  80, 100, EF_NONE,      0, 0, 0, TG_FOE),  # LATER (Gen 6)
    ("PLAY ROUGH",   "play-rough",    'fairy',    MC_PHYS,  90,  90, EF_NONE,      0, 0, 0, TG_FOE),  # LATER (Gen 6)
    ("MOONBLAST",    "moonblast",     'fairy',    MC_SPEC,  95, 100, EF_NONE,      0, 0, 0, TG_FOE),  # LATER (Gen 6)

    # --- stat stages, self ------------------------------------------------
    # The player's answer to a gym leader with perfect IVs: you are behind on
    # raw stats by design, so you win by setting up or by type advantage.
    # Stages reset on switch out, which is what makes switching a decision.
    ("SWORDS DANCE", "swords-dance",  'normal',   MC_STATUS, 0,   0, EF_STAGE,     0, ST_ATK,          2, TG_SELF),
    ("AGILITY",      "agility",       'psychic',  MC_STATUS, 0,   0, EF_STAGE,     0, ST_SPE,          2, TG_SELF),
    ("BARRIER",      "barrier",       'psychic',  MC_STATUS, 0,   0, EF_STAGE,     0, ST_DEF,          2, TG_SELF),
    ("AMNESIA",      "amnesia",       'psychic',  MC_STATUS, 0,   0, EF_STAGE,     0, ST_SPD,          2, TG_SELF),
    ("NASTY PLOT",   "nasty-plot",    'dark',     MC_STATUS, 0,   0, EF_STAGE,     0, ST_SPA,          2, TG_SELF),  # LATER (Gen 4)
    ("DRAGON DANCE", "dragon-dance",  'dragon',   MC_STATUS, 0,   0, EF_STAGE,     0, ST_ATK | ST_SPE, 1, TG_SELF),  # LATER (Gen 2)
    ("BULK UP",      "bulk-up",       'fighting', MC_STATUS, 0,   0, EF_STAGE,     0, ST_ATK | ST_DEF, 1, TG_SELF),  # LATER (Gen 3)
    # --- stat stages, foe -------------------------------------------------
    ("GROWL",        "growl",         'normal',   MC_STATUS, 0,   0, EF_STAGE,     0, ST_ATK,         -1, TG_FOE),
    ("LEER",         "leer",          'normal',   MC_STATUS, 0,   0, EF_STAGE,     0, ST_DEF,         -1, TG_FOE),
    ("SCREECH",      "screech",       'normal',   MC_STATUS, 0,   0, EF_STAGE,     0, ST_DEF,         -2, TG_FOE),
    ("STRING SHOT",  "string-shot",   'bug',      MC_STATUS, 0,   0, EF_STAGE,     0, ST_SPE,         -1, TG_FOE),
    # --- healing ----------------------------------------------------------
    ("RECOVER",      "recover",       'normal',   MC_STATUS, 0,   0, EF_HEAL,     50, 0, 0, TG_SELF),
    ("SOFT-BOILED",  "soft-boiled",   'normal',   MC_STATUS, 0,   0, EF_HEAL,     50, 0, 0, TG_SELF),

    # --- fallback ---------------------------------------------------------
    # Not in any learnset (slug None keeps the fetcher off it): the engine
    # substitutes this when a species has nothing else. METAPOD, KAKUNA and
    # DITTO learn none of the 70 above and would otherwise be unable to act.
    # For the first two that is a few hours before they evolve; DITTO is a
    # final form and stays this way, since its entire movepool is TRANSFORM.
    ("STRUGGLE",     None,            'normal',   MC_PHYS,  50,   0, EF_RECOIL,    4, 0, 0, TG_FOE),

    # --- APPEND-ONLY BELOW HERE -------------------------------------------
    # A move's index IS its position in this list (gen_moves.py: idx = i + 1),
    # and Pet.moves[]/PartyMon.moves[] store that index RAW in NVS. Inserting a
    # move into its type section above would shift every move after it by one
    # and silently rewrite the moveset of every creature already saved -- the
    # live pet and every banked party member. So a new move goes HERE, at the
    # end, however untidy that looks beside the type grouping.
    #
    # DARK PULSE, for DARKRAI (491), which dexdata_test caught with no same-type
    # attack once Sinnoh landed. BITE and CRUNCH are the only other Dark moves
    # and both are PHYSICAL, so every special-attacking Dark type had no special
    # STAB; Darkrai is SpA 135 / Atk 90, so neither was usable to it. It learns
    # DARK PULSE by level-up at 27, so this is its real move, not a contrivance.
    ("DARK PULSE",   "dark-pulse",    'dark',     MC_SPEC,  80, 100, EF_NONE,      0, 0, 0, TG_FOE),  # LATER (Gen 4)

    # The STATUS moves (v3.33). Until these, every ailment rode on a damaging
    # move's secondary chance and AIL_SLEEP was inflicted by nothing. Each is a
    # sure ailment if it hits (chance 100); the accuracy is the games'. Who can
    # learn them comes from tools/fetch_status_learnsets.py (TM-style, so
    # gated at TM_LEVEL like every other TM).
    ("THUNDER WAVE", "thunder-wave",  'electric', MC_STATUS, 0,  90, EF_NONE,      0, 0, 0, TG_FOE, AIL_PARA, 100),
    ("SLEEP POWDER", "sleep-powder",  'grass',    MC_STATUS, 0,  75, EF_NONE,      0, 0, 0, TG_FOE, AIL_SLEEP, 100),
    ("HYPNOSIS",     "hypnosis",      'psychic',  MC_STATUS, 0,  60, EF_NONE,      0, 0, 0, TG_FOE, AIL_SLEEP, 100),
    ("TOXIC",        "toxic",         'poison',   MC_STATUS, 0,  90, EF_NONE,      0, 0, 0, TG_FOE, AIL_POISON, 100),
    ("WILL-O-WISP",  "will-o-wisp",   'fire',     MC_STATUS, 0,  85, EF_NONE,      0, 0, 0, TG_FOE, AIL_BURN, 100),
    ("CONFUSE RAY",  "confuse-ray",   'ghost',    MC_STATUS, 0, 100, EF_NONE,      0, 0, 0, TG_FOE, AIL_CONFUSE, 100),
]

# Accuracy drops (SAND ATTACK, SMOKESCREEN, FLASH) were left out here because
# accuracy stages did not exist; they do since v3.37 (ST_ACC/ST_EVA), and those
# moves arrived with the second batch.

# The Gen 1-7 moves the engine can play (v3.33), APPENDED after everything
# above: tools/pick_moves.py chose and wrote them once, and that file is
# append-only too. MOVES_BASE is the hand-picked list on its own, which the
# picker reads to know what is already here.
MOVES_BASE = MOVES
try:
    from dex_moves_more import MORE_MOVES   # noqa: E402 (it reads the constants above)
except ImportError:   # before pick_moves.py has written it
    MORE_MOVES = []
# ...and the second batch (v3.37, tools/pick_moves2.py), after those.
try:
    from dex_moves_more2 import MORE2_MOVES
except ImportError:
    MORE2_MOVES = []
MOVES = MOVES_BASE + MORE_MOVES + MORE2_MOVES

SLUG_TO_NAME = {slug: name for name, slug, *_ in MOVES if slug}
