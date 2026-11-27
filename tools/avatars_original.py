"""Original avatars, drawn for this project in the Gen 1 overworld style.

Hand-authored pixel art, NOT taken from any game: 16x16, front-facing, the same
three-shade-plus-transparent palette as the pret sprites so they sit beside
them in the picker. gen_avatars.py APPENDS these after its SHOP list -- the
order here is FROZEN once committed, exactly like SHOP (ownership is a bit per
shop index). New ones go at the END.

The 157 creature costumes (starters, final forms, Eeveelutions, Mimikyu and
every legendary) are composed in avatars_costume.py and come AFTER these.
AVATAR_COUNT is a uint8_t index, so the whole set tops out at 255.

Grid legend:  '.' transparent   '#' outline   'a' accent   'w' highlight
Labels <= 10 characters, unaccented ASCII.
"""

ORIGINAL = [
    ('NINJA', 0xD8A3, [
        '......####......',
        '....########....',
        '...##########...',
        '..############..',
        '..aaaaaaaaaaaa..',
        '.##############a',
        '.##wwwwwwwwww##a',
        '.##ww#wwww#ww##.',
        '..############..',
        '..###aaaaaa###..',
        '.#w##########w#.',
        '.#ww########ww#.',
        '..####aaaa####..',
        '...##########...',
        '...####..####...',
        '....###..###....',
    ]),
    ('PIKA HOOD', 0xFEA0, [
        '.##..........##.',
        '.##..........##.',
        '.#a#........#a#.',
        '.#aa#.####.#aa#.',
        '..#aa#aaaa#aa#..',
        '..#aaaaaaaaaa#..',
        '.#aa#wwwwww#aa#.',
        '.#a#ww#ww#ww#a#.',
        '.#a#ww#ww#ww#a#.',
        '..###wwwwww###..',
        '.#aa########aa#.',
        '.#ww#aaaaaa#ww#.',
        '..###aaaaaa###..',
        '...##########...',
        '...#aaa##aaa#...',
        '....###..###....',
    ]),
    ('BALL CAP', 0xF800, [
        '.....######.....',
        '....#aaaaaa#....',
        '...#aaa##aaa#...',
        '...####ww####...',
        '...#www##www#...',
        '..############..',
        '.#w#wwwwwwww#w#.',
        '.#wwww#ww#wwww#.',
        '..##ww#ww#ww##..',
        '..###wwaaww###..',
        '.#ww#aaaaaa#ww#.',
        '.#ww#aaaaaa#ww#.',
        '..###aaaaaa###..',
        '...##########...',
        '...#aaa##aaa#...',
        '....###..###....',
    ]),
    ('GHOST HOOD', 0x895F, [
        '..#..........#..',
        '.#a#...##...#a#.',
        '.#aa#.#aa#.#aa#.',
        '.#aaa#aaaa#aaa#.',
        '..#aaaaaaaaaa#..',
        '..#a#wwwwww#a#..',
        '.#a#wwwwwwww#a#.',
        '.#a#w##ww##w#a#.',
        '.#aa#wwwwww#aa#.',
        '..##w#w##w#w##..',
        '.#aa########aa#.',
        '.#ww#aaaaaa#ww#.',
        '..###aaaaaa###..',
        '...##########...',
        '...#aaa##aaa#...',
        '....###..###....',
    ]),
    ('WIZARD', 0x51DF, [
        '.........##.....',
        '........#aa#....',
        '.......#aaa#....',
        '......#aawa#....',
        '.....#aaaaaa#...',
        '...##########...',
        '.##aaaaaaaaaa##.',
        '..############..',
        '.#w#wwwwwwww#w#.',
        '.#wwww#ww#wwww#.',
        '..##wwwwwwww##..',
        '.#aa#wwwwww#aa#.',
        '.#aaa######aaa#.',
        '..#aaaaaaaaaa#..',
        '.#aaaaaaaaaaaa#.',
        '..############..',
    ]),
    ('PIRATE', 0xC104, [
        '.##..........##.',
        '.#a##########a#.',
        '..#aaaawwaaaa#..',
        '...#aaaaaaaa#...',
        '..############..',
        '..#wwwwwwwwww#..',
        '.#w###wwwwwww#..',
        '.#w###ww#wwww#..',
        '..##w#wwwwww##..',
        '..###wwwwww###..',
        '.#ww#aaaaaa#ww#.',
        '.#ww#wwwwww#ww#.',
        '..###aaaaaa###..',
        '...##########...',
        '...#www##www#...',
        '....###..###....',
    ]),
    ('KNIGHT', 0xA534, [
        '.......##.......',
        '......#aa#......',
        '.....######.....',
        '....#wwwwww#....',
        '...#wwwwwwww#...',
        '..#wwwwwwwwww#..',
        '..############..',
        '..#w##w##w##w#..',
        '..#wwwwwwwwww#..',
        '..############..',
        '.#ww#aaaaaa#ww#.',
        '.#ww#aa##aa#ww#.',
        '..###aaaaaa###..',
        '...##########...',
        '...#www##www#...',
        '....###..###....',
    ]),
    ('ASTRONAUT', 0x4A5F, [
        '.....######.....',
        '...##wwwwww##...',
        '..#wwwwwwwwww#..',
        '.#wwwwwwwwwwww#.',
        '.#ww########ww#.',
        '.#w#aaaaaaaa#w#.',
        '.#w#awwaaaaa#w#.',
        '.#w#aaaaaaaa#w#.',
        '.#ww########ww#.',
        '..#wwwwwwwwww#..',
        '.#ww#wwwwww#ww#.',
        '.#ww#w#aa#w#ww#.',
        '..###wwwwww###..',
        '...##########...',
        '...#www##www#...',
        '....###..###....',
    ]),
]

# STARTERS: more of the free starting set, so a new player can pick a trainer
# who looks like them -- five boys and five girls counting RED, BLUE and GIRL.
# Unlocked from rank 1, never sold. Unlike everything above they are NOT shop
# entries: gen_avatars.py emits them after the costumes and outside the shop
# list, so no shop index (a bit in casino.cpp) moves. FROZEN order, append only.
#
# They carry a full palette rather than one accent, because skin, hair and
# clothes are separate colours here. Legend:
#   '.' transparent  '#' outline  's' skin  'h' hair (or headwear)
#   'c' top  'd' trousers/skirt  'w' white  'k' cap black
SKIN_LIGHT, SKIN_MED, SKIN_TAN, SKIN_BROWN, SKIN_DARK = 0xF655, 0xCCAD, 0xC46C, 0xA369, 0x59E5
HAIR_BLACK = 0x2104
STARTER_BODY_TROUSERS = [
    '.##cccccccccc##.',
    '.#s#cccccccc#s#.',
    '.#s#cccccccc#s#.',
    '..##dddddddd##..',
    '...#ddd##ddd#...',
    '....###..###....',
]
STARTER_BODY_SKIRT = [
    '.##cccccccccc##.',
    '.#s#cccccccc#s#.',
    '..##cccccccc##..',
    '..#dddddddddd#..',
    '....#s#..#s#....',
    '....###..###....',
]
STARTERS = [
    # tan, bald, a black cap with a white NY on it, white tee, jeans
    ('NY CAP', {'s': SKIN_TAN, 'k': 0x0000, 'w': 0xFFFF, 'c': 0xFFFF, 'd': 0x3B35}, [
        '....########....',
        '...#wwkwkwkw#...',
        '...#wkwwkkwk#...',
        '...#wkkwkkwk#...',
        '..############..',
        '..#ssssssssss#..',
        '..#ss#ssss#ss#..',
        '..#ssssssssss#..',
        '...#sss##sss#...',
        '....##ssss##....',
    ] + STARTER_BODY_TROUSERS),
    # dark skin, black curls, red hoodie
    ('CURLS', {'s': SKIN_DARK, 'h': HAIR_BLACK, 'c': 0xD8A3, 'd': 0x4208}, [
        '....##.##.##....',
        '...#hh#hh#hh#...',
        '..#hhhhhhhhhh#..',
        '.#hhhhhhhhhhhh#.',
        '.#hhhhhhhhhhhh#.',
        '.#hssssssssssh#.',
        '..#ss#ssss#ss#..',
        '..#ssssssssss#..',
        '...#sss##sss#...',
        '....##ssss##....',
    ] + STARTER_BODY_TROUSERS),
    # light skin, brown side-part, glasses, green jacket
    ('GLASSES', {'s': SKIN_LIGHT, 'h': 0x7A23, 'w': 0xFFFF, 'c': 0x3CAA, 'd': 0x298C}, [
        '.....######.....',
        '....#hhhhhh#....',
        '...#hhhhhhhh#...',
        '..#hhhhhhhhhh#..',
        '..#hhhhhhhsss#..',
        '..#ssssssssss#..',
        '..#s#w####w#s#..',
        '..#ssssssssss#..',
        '...#sss##sss#...',
        '....##ssss##....',
    ] + STARTER_BODY_TROUSERS),
    # light skin, blonde ponytail, pink top, blue skirt
    ('PONYTAIL', {'s': SKIN_LIGHT, 'h': 0xF64A, 'c': 0xF3D5, 'd': 0x4379}, [
        '.....######.....',
        '....#hhhhhh#....',
        '...#hhhhhhhh#.##',
        '..#hhhhhhhhhh#h#',
        '..#hssssssssh#h#',
        '..#ssssssssss##.',
        '..#ss#ssss#ss#..',
        '..#ssssssssss#..',
        '...#sss##sss#...',
        '....##ssss##....',
    ] + STARTER_BODY_SKIRT),
    # dark skin, black braids with white ties, yellow top, denim skirt
    ('BRAIDS', {'s': SKIN_BROWN, 'h': HAIR_BLACK, 'w': 0xFFFF, 'c': 0xF645, 'd': 0x3B35}, [
        '.....######.....',
        '....#hhhhhh#....',
        '...#hhhhhhhh#...',
        '..#hhhhhhhhhh#..',
        '..#hssssssssh#..',
        '.#h#ssssssss#h#.',
        '.#h#s#ssss#s#h#.',
        '.#h#ssssssss#h#.',
        '.#h#sss##sss#h#.',
        '.#w#.#ssss#.#w#.',
    ] + STARTER_BODY_SKIRT),
    # medium skin, black bob, teal top, dark trousers
    ('BOB', {'s': SKIN_MED, 'h': HAIR_BLACK, 'c': 0x2D55, 'd': 0x4208}, [
        '.....######.....',
        '...##hhhhhh##...',
        '..#hhhhhhhhhh#..',
        '.#hhhhhhhhhhhh#.',
        '.#hhsssssssshh#.',
        '.#hssssssssssh#.',
        '.#hss#ssss#ssh#.',
        '.#hssssssssssh#.',
        '.#h#sss##sss#h#.',
        '..##.#ssss#.##..',
    ] + STARTER_BODY_TROUSERS),
    # brown skin, purple hijab, light top, long navy skirt
    ('HIJAB', {'s': SKIN_BROWN, 'h': 0x8236, 'c': 0xE73C, 'd': 0x298C}, [
        '.....######.....',
        '....#hhhhhh#....',
        '...#hhhhhhhh#...',
        '..#hhhhhhhhhh#..',
        '..#hh######hh#..',
        '.#hh#ssssss#hh#.',
        '.#hh#s#ss#s#hh#.',
        '.#hh#ss##ss#hh#.',
        '.#hhh#ssss#hhh#.',
        '.#hhhh####hhhh#.',
        '.#hhhhhhhhhhhh#.',
        '.#s#cccccccc#s#.',
        '..##cccccccc##..',
        '..#dddddddddd#..',
        '...#dddddddd#...',
        '....###..###....',
    ]),
    # light skin, long black hair, glasses, lavender top, dark skirt
    ('SPECS', {'s': SKIN_LIGHT, 'h': HAIR_BLACK, 'w': 0xFFFF, 'c': 0x933B, 'd': 0x4208}, [
        '.....######.....',
        '...##hhhhhh##...',
        '..#hhhhhhhhhh#..',
        '.#hhhhhhhhhhhh#.',
        '.#hhsssssssshh#.',
        '.#hssssssssssh#.',
        '.#hs#w####w#sh#.',
        '.#hssssssssssh#.',
        '.#h#sss##sss#h#.',
        '.#hh##ssss##hh#.',
    ] + STARTER_BODY_SKIRT),
    # medium skin, black faux-hawk with faded sides, orange tee, jeans
    ('FOHAWK', {'s': SKIN_MED, 'h': HAIR_BLACK, 'c': 0xF465, 'd': 0x3B35}, [
        '......####......',
        '.....#hhhh#.....',
        '....#hhhhhh#....',
        '...#shhhhhhs#...',
        '..#sshhhhhhss#..',
        '..#ssssssssss#..',
        '..#ss#ssss#ss#..',
        '..#ssssssssss#..',
        '...#sss##sss#...',
        '....##ssss##....',
    ] + STARTER_BODY_TROUSERS),
]

# YOU: the customisable trainer. Its pixels are COMPOSED at draw time from the
# wardrobe (wardrobe.cpp, wardrobe_art.h); this grid is only the stored stand-in
# -- the look a new save starts with -- for anything that reads the table raw.
# Emitted after STARTERS with its own #define, AVATAR_YOU. Never sold.
YOU = ('CUSTOM', {'s': 0xCCAD, 'h': 0x7A23, 'e': 0x6A25, 'c': 0x2B7F, 'd': 0x3B35, 'f': 0xD8A3}, [
    '................',
    '................',
    '.....######.....',
    '....#hhhhhh#....',
    '...#hhhhhhhh#...',
    '..#hhhhhhhhhh#..',
    '..#hsessssesh#..',
    '..#ssssssssss#..',
    '...#sss##sss#...',
    '....##ssss##....',
    '.##cccccccccc##.',
    '.#s#cccccccc#s#.',
    '.#s#cccccccc#s#.',
    '..##dddddddd##..',
    '...#ddd##ddd#...',
    '...#fff..fff#...',
])
