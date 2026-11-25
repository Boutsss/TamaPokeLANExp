"""Costume avatars: a kid in a creature onesie, built from a parts kit.

Original art for this project, in the 16x16 Gen 1 overworld style. Every
costume is the same hooded kid (BASE) with head ornaments, a face and body
overlays chosen per species, and the species' accent colour. Composed rather
than hand-drawn one by one because there are 159 of them.

Overlay rows: '.' leaves the pixel alone, anything else writes it.
Legend: '#' outline  'a' accent  'w' highlight  '_' erase to transparent.

THE ORDER OF COSTUMES IS FROZEN once committed -- they are Game Corner prizes
and ownership is a bit per shop index. Append only.
"""
import os
import re

BASE = [
    '................',
    '................',
    '................',
    '.....######.....',
    '...##aaaaaa##...',
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
]

# Head ornaments: (first row, rows)
HEAD = {
    'pointy': (0, ['.##..........##.', '.#a#........#a#.', '.#aa#......#aa#.', '..#aa#....#aa#..']),
    'tipped': (0, ['.##..........##.', '.###........###.', '.#aa#......#aa#.', '..#aa#....#aa#..']),
    'long':   (0, ['..##........##..', '..#a#......#a#..', '..#aw#....#wa#..', '..#aa#....#aa#..']),
    'side':   (0, ['................', '##............##', '#aa#........#aa#', '.#aa#......#aa#.', '..#aa#....#aa#..']),
    'round':  (1, ['..###......###..', '.#aaa#....#aaa#.', '.#awa#....#awa#.']),
    'horn':   (0, ['.......##.......', '......#ww#......', '......#ww#......', '.....#wwww#.....']),
    'horns':  (0, ['..##........##..', '..#w#......#w#..', '...#w#....#w#...', '....#w#..#w#....']),
    'nubs':   (1, ['...##......##...', '...#a#....#a#...', '....#a#..#a#....']),
    'crest':  (0, ['......#..#......', '.....#a##a#.....', '......#aa#......', '.......##.......']),
    'fin':    (0, ['.......#........', '......#a#.......', '......#aa#......', '.....#aaa#......']),
    'leaf':   (0, ['........###.....', '.......#aaa#....', '......#aaw#.....', '.......##.......']),
    'flame':  (0, ['......#..#......', '.....#w##w#.....', '.....#wwww#.....', '......#ww#......']),
    'blaze':  (0, ['..#..#....#..#..', '.#w##w#..#w##w#.', '..#ww#.##.#ww#..', '...##.#ww#.##...']),
    'crown':  (0, ['................', '...#...##...#...', '..#a#.#aa#.#a#..', '..#aa#aaaa#aa#..']),
    'antenna': (0, ['..#..........#..', '..##........##..', '...#........#...', '....#......#....']),
    'wings':  (1, ['#..............#', '##............##', '#a#..........#a#', '#aa#........#aa#']),
    'bow':    (0, ['................', '..........#.#...', '.........#w#w#..', '..........###...']),
    'spikes': (0, ['..#.#.#..#.#.#..', '..#a#a#..#a#a#..', '...#aa####aa#...']),
    'bulb':   (0, ['......####......', '....##aaaa##....', '...#awa##aaa#...', '...#aaa##aaa#...']),
    'flower': (0, ['...###....###...', '..#aaa#..#aaa#..', '..#awa####awa#..', '...##aaaaaa##...']),
    'tree':   (0, ['.....######.....', '....#awaaaa#....', '.....######.....', '.......##.......']),
    'cannons': (0, ['................', '..###......###..', '..#ww#....#ww#..', '..####....####..']),
    'star':   (0, ['.......##.......', '......#ww#......', '.....#wwww#.....', '......#ww#......']),
    'tall':   (0, ['.......##.......', '......#aa#......', '.....#aaaa#.....', '....#aaaaaa#....']),
    'halo':   (0, ['....########....', '...#wwwwwwww#...', '....########....']),
    'antlers': (0, ['.#.#........#.#.', '.#a#.#....#.#a#.', '..#a#a#..#a#a#..', '...#aa#..#aa#...']),
    'orb':    (0, ['......####......', '.....#wwww#.....', '.....#w##w#.....', '......####......']),
    'floppy': (0, ['...##...........', '..#aa#..........', '..#aaa#....##...', '...#aa#...#aa#..']),
    'gem':    (5, ['.......ww.......']),
    'hood':   (2, ['.....######.....', '....#aaaaaa#....']),
}

# Faces replace rows 6-9.
FACE = {
    'kid':    None,
    'fierce': ['.#aa#wwwwww#aa#.', '.#a##w#ww#w##a#.', '.#a#ww#ww#ww#a#.', '..###wwwwww###..'],
    'sleepy': ['.#aa#wwwwww#aa#.', '.#a#ww####ww#a#.', '.#a#wwwwwwww#a#.', '..###ww##ww###..'],
    'mask':   ['.#aa#wwwwww#aa#.', '.#a##########a#.', '.#a#w#wwww#w#a#.', '..###wwwwww###..'],
    'visor':  ['.#aa########aa#.', '.#a#wwww####a#a#', '.#a##########a#.', '..###wwwwww###..'],
    'beak':   ['.#aa#wwwwww#aa#.', '.#a#ww#ww#ww#a#.', '.#a#ww#aa#ww#a#.', '..###wwaawww##..'],
    'jaw':    ['.#aa#wwwwww#aa#.', '.#a#ww#ww#ww#a#.', '.#a##########a#.', '..##w#w##w#w##..'],
    'snout':  ['.#aa#wwwwww#aa#.', '.#a#ww#ww#ww#a#.', '.#a#ww#aa#ww#a#.', '..###w#aa#w###..'],
    'braille': ['.#aa#wwwwww#aa#.', '.#a#w#w##w#w#a#.', '.#a#ww#ww#ww#a#.', '..###w#ww#w###..'],
    'scribble': ['.#aa#wwwwww#aa#.', '.#a#w##ww##w#a#.', '.#a#wwwwwwww#a#.', '..###w#ww#w###..'],
}

# Body overlays: (first row, rows)
BODY = {
    'suit':   (10, []),
    'belly':  (11, ['.#ww#awwwwa#ww#.', '..###wwwwww###..']),
    'spots':  (11, ['.#ww#a#aa#a#ww#.']),
    'stripe': (11, ['.#ww#wwwwww#ww#.']),
    'collar': (10, ['.#aa#w#ww#w#aa#.']),
    'scarf':  (10, ['.#aawwwwwwwwaa#.']),
    'belt':   (12, ['..###wwwwww###..']),
    'shell':  (11, ['.#ww#w#ww#w#ww#.', '..###wwwwww###..']),
    'rings':  (11, ['.#ww#awwwwa#ww#.']),
    'tail':   (9, ['..............#.', '.............#a#', '..............#a', '...............#']),
    'flametail': (9, ['..............#.', '.............#w#', '..............#w', '...............#']),
    'wings':  (9, ['#..............#', '##aa########aa##', '#a#w#......#w#a#', '_#............#_']),
    'cape':   (13, ['..#aaaaaaaaaa#..', '.#aaa#aaaa#aaa#.', '..###......###..']),
    'rag':    (12, ['..#aaaaaaaaaa#..', '.#aaaaaaaaaaaa#.', '.#a#aa#aa#aa#a#.', '..#_##_##_##_#..']),
}

# (dex, '#RRGGBB', head parts, face, body parts)
STARTERS = [
    (1, '#5CC9A0', 'bulb', 'kid', 'spots'),
    (4, '#F08030', '', 'kid', 'belly flametail'),
    (7, '#6CB8F0', '', 'kid', 'shell tail'),
    (152, '#A8E060', 'leaf', 'kid', 'collar'),
    (155, '#3A5A78', 'flame', 'sleepy', 'belly'),
    (158, '#4890E0', 'spikes', 'jaw', 'belly'),
    (252, '#60C060', 'crest', 'kid', 'belly tail'),
    (255, '#F89030', 'crest', 'beak', 'wings'),
    (258, '#58A8E8', 'fin', 'kid', 'belly'),
    (387, '#80C050', 'leaf', 'kid', 'shell'),
    (390, '#E08040', 'flame', 'kid', 'belly tail'),
    (393, '#68A8E8', 'crest', 'beak', 'belly wings'),
    (495, '#50B050', 'side', 'kid', 'collar tail'),
    (498, '#F07030', 'round', 'snout', 'tail'),
    (501, '#80C8F0', 'round', 'kid', 'shell'),
    (650, '#78A048', 'spikes', 'kid', 'belly'),
    (653, '#F0C050', 'long', 'kid', 'collar tail'),
    (656, '#78B8F0', 'round', 'kid', 'scarf'),
    (722, '#C0A070', 'bow', 'beak', 'wings'),
    (725, '#404048', 'pointy', 'kid', 'stripe'),
    (728, '#70B0F0', '', 'snout', 'collar'),
    (810, '#60B040', 'horn', 'kid', 'tail'),
    (813, '#E05030', 'long', 'kid', 'belly'),
    (816, '#60A8E0', 'fin', 'sleepy', 'tail'),
    (906, '#78C860', 'pointy', 'kid', 'tail'),
    (909, '#E85838', 'flame', 'jaw', 'belly'),
    (912, '#70B8F0', 'crest', 'beak', 'wings'),
]
FINALS = [
    (3, '#3E9E80', 'flower', 'fierce', 'spots'),
    (6, '#F08030', 'horns', 'fierce', 'wings flametail'),
    (9, '#4870C8', 'cannons', 'fierce', 'shell'),
    (154, '#70C050', 'flower', 'kid', 'collar'),
    (157, '#304868', 'blaze', 'fierce', 'belly'),
    (160, '#3070C8', 'spikes', 'jaw', 'tail'),
    (254, '#40A040', 'leaf', 'fierce', 'spots tail'),
    (257, '#E04828', 'horns', 'beak', 'belly'),
    (260, '#4878C8', 'fin', 'fierce', 'belly'),
    (389, '#608840', 'tree', 'fierce', 'shell'),
    (392, '#E07030', 'blaze', 'fierce', 'tail'),
    (395, '#3858A8', 'crown', 'beak', 'wings'),
    (497, '#50A050', 'side', 'fierce', 'collar'),
    (500, '#E04828', 'round', 'snout', 'belt'),
    (503, '#4880C0', 'horn', 'fierce', 'shell'),
    (652, '#6A8A40', 'spikes', 'fierce', 'shell'),
    (655, '#D04030', 'long', 'kid', 'cape'),
    (658, '#3050A0', 'hood', 'mask', 'scarf'),
    (724, '#78905A', 'hood', 'mask', 'wings'),
    (727, '#C83A3A', 'pointy', 'fierce', 'belt'),
    (730, '#70B0E8', 'bow', 'kid', 'collar'),
    (812, '#4E8A40', 'crest', 'fierce', 'belly'),
    (815, '#E86040', 'long', 'fierce', 'stripe'),
    (818, '#3A78C8', 'fin', 'visor', 'cape'),
    (908, '#5AA050', 'pointy', 'mask', 'cape'),
    (911, '#D04838', 'blaze', 'jaw', 'belly'),
    (914, '#48A0E0', 'crown', 'beak', 'wings'),
]
EEVEELUTIONS = [
    (134, '#58A8E8', 'fin', 'kid', 'collar tail'),
    (135, '#F8D030', 'spikes', 'kid', 'collar'),
    (136, '#F08030', 'side flame', 'kid', 'collar'),
    (196, '#D8A0E0', 'side gem', 'kid', 'tail'),
    (197, '#303038', 'long', 'kid', 'rings'),
    (470, '#A8D870', 'side leaf', 'kid', 'tail'),
    (471, '#70C8E0', 'side', 'kid', 'collar'),
    (700, '#F0A0C0', 'side bow', 'kid', 'collar'),
]
MIMIKYU = [(778, '#E8D070', 'floppy', 'scribble', 'rag')]
LEGENDARIES = [
    (144, '#78C8F0', 'crest', 'beak', 'wings'),
    (145, '#F8D030', 'spikes', 'beak', 'wings'),
    (146, '#F08030', 'flame', 'beak', 'wings'),
    (150, '#B8A0C8', 'nubs', 'fierce', 'tail'),
    (151, '#F8B0C8', 'round', 'kid', 'tail'),
    (243, '#F8D030', 'crown', 'fierce', 'stripe'),
    (244, '#B06030', 'blaze', 'fierce', 'collar'),
    (245, '#60B8E8', 'crest gem', 'kid', 'cape'),
    (249, '#A8B8E0', 'fin', 'kid', 'belly wings'),
    (250, '#E85030', 'crest', 'beak', 'wings flametail'),
    (251, '#98D870', 'antenna', 'kid', 'wings'),
    (377, '#B08850', '', 'braille', 'belly'),
    (378, '#90D8F0', 'spikes', 'braille', 'belly'),
    (379, '#A0A0B0', '', 'braille', 'stripe'),
    (380, '#E06060', 'wings', 'kid', 'belly'),
    (381, '#5878D0', 'wings', 'fierce', 'wings'),
    (382, '#3868C8', 'fin', 'fierce', 'spots'),
    (383, '#C83828', 'spikes', 'fierce', 'tail'),
    (384, '#40A060', 'horns', 'fierce', 'stripe tail'),
    (385, '#F0E070', 'star', 'sleepy', 'collar'),
    (386, '#E07040', 'tall', 'visor', 'stripe'),
    (480, '#F0D070', 'side gem', 'sleepy', 'tail'),
    (481, '#F0A0C0', 'side gem', 'kid', 'tail'),
    (482, '#6898E0', 'pointy gem', 'fierce', 'tail'),
    (483, '#4868B8', 'crown gem', 'fierce', 'stripe'),
    (484, '#D8A0C0', 'wings gem', 'fierce', 'spots'),
    (485, '#A06040', '', 'visor', 'spots'),
    (486, '#D8D0A8', '', 'braille', 'spots'),
    (487, '#585868', 'horns', 'fierce', 'wings'),
    (488, '#F0D070', 'halo', 'kid', 'cape'),
    (489, '#80C8F0', 'horn', 'kid', 'belly'),
    (490, '#50A8E8', 'antenna', 'kid', 'belly'),
    (491, '#383840', 'side', 'mask', 'cape'),
    (492, '#90D070', 'flower', 'kid', 'collar'),
    (493, '#C8C098', 'crown halo', 'fierce', 'belt'),
    (494, '#F09040', 'long', 'kid', 'wings'),
    (638, '#4890C8', 'horn', 'fierce', 'cape'),
    (639, '#A08060', 'horns', 'fierce', 'collar'),
    (640, '#60C060', 'horn', 'kid', 'cape'),
    (641, '#60A060', 'horns', 'fierce', 'rag'),
    (642, '#5878C8', 'spikes', 'fierce', 'rag'),
    (643, '#B8C0D8', 'crest', 'kid', 'wings tail'),
    (644, '#404850', 'spikes', 'fierce', 'wings tail'),
    (645, '#D09050', 'horns', 'kid', 'rag'),
    (646, '#8898B0', 'spikes', 'fierce', 'tail'),
    (647, '#F0E060', 'horn', 'kid', 'collar'),
    (648, '#90D898', 'side', 'sleepy', 'cape'),
    (649, '#A060C0', 'cannons', 'visor', 'stripe'),
    (716, '#4878C8', 'antlers', 'kid', 'tail'),
    (717, '#C03838', 'crest', 'fierce', 'wings'),
    (718, '#50A040', 'tall', 'visor', 'stripe'),
    (719, '#F0A0C0', 'crown gem', 'kid', 'cape'),
    (720, '#B070C8', 'horns halo', 'fierce', 'belt'),
    (721, '#C86040', 'cannons', 'fierce', 'belly'),
    (772, '#808890', 'crest', 'visor', 'belly'),
    (773, '#A0A8B8', 'crest', 'fierce', 'tail'),
    (785, '#F0A030', 'crest', 'mask', 'shell'),
    (786, '#F0A0C0', 'antenna', 'mask', 'shell'),
    (787, '#D8B040', 'horns', 'mask', 'shell'),
    (788, '#A8A0E0', 'fin', 'mask', 'shell'),
    (789, '#3848A0', 'side', 'kid', 'rag'),
    (790, '#F0C050', 'halo', 'sleepy', 'belly'),
    (791, '#E0A030', 'crown', 'fierce', 'tail'),
    (792, '#483878', 'wings halo', 'kid', 'wings'),
    (800, '#303040', 'spikes', 'visor', 'spots'),
    (801, '#C8A060', 'round', 'kid', 'cape'),
    (802, '#505058', 'flame', 'mask', 'tail'),
    (807, '#E8C040', 'pointy', 'fierce', 'stripe'),
    (808, '#B0B0C0', 'orb', 'kid', 'belly'),
    (809, '#A0A0B0', 'orb', 'fierce', 'stripe'),
    (888, '#4888D8', 'pointy', 'fierce', 'scarf'),
    (889, '#C83848', 'pointy', 'fierce', 'shell'),
    (890, '#682878', 'spikes', 'visor', 'wings'),
    (891, '#D0B890', 'round', 'kid', 'belt'),
    (892, '#505060', 'round', 'fierce', 'belt cape'),
    (893, '#404850', 'leaf', 'fierce', 'tail'),
    (894, '#F0D030', '', 'braille', 'wings'),
    (895, '#A04060', 'horns', 'braille', 'belly'),
    (896, '#A0D0F0', 'crown', 'fierce', 'collar'),
    (897, '#404048', 'side', 'mask', 'collar'),
    (898, '#6050A0', 'crown', 'sleepy', 'cape'),
    (905, '#E880A8', 'horns', 'kid', 'rag'),
    (1001, '#6A5A40', 'leaf', 'fierce', 'shell'),
    (1002, '#A8C8E0', 'pointy', 'fierce', 'stripe'),
    (1003, '#7A5A3A', 'antlers', 'fierce', 'collar'),
    (1004, '#C84830', 'flame', 'fierce', 'tail'),
    (1007, '#D04030', 'side', 'fierce', 'belly'),
    (1008, '#5848C0', 'crest', 'visor', 'tail'),
    (1014, '#405040', 'spikes', 'fierce', 'belt'),
    (1015, '#8050A0', 'pointy', 'mask', 'tail'),
    (1016, '#503848', 'crest', 'beak', 'wings'),
    (1017, '#50A050', 'horns', 'mask', 'cape'),
    (1024, '#4890C8', 'halo gem', 'kid', 'shell'),
    (1025, '#A050C0', 'crown', 'sleepy', 'rag'),
]
# Frozen order: starters, their final forms, the Eeveelutions, Mimikyu, the legendaries.
COSTUMES = STARTERS + FINALS + EEVEELUTIONS + MIMIKYU + LEGENDARIES


def rgb565(h):
    r, g, b = int(h[1:3], 16), int(h[3:5], 16), int(h[5:7], 16)
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def overlay(grid, row, rows):
    for dy, line in enumerate(rows):
        assert len(line) == 16, line
        for x, ch in enumerate(line):
            if ch == '.':
                continue
            grid[row + dy][x] = '.' if ch == '_' else ch


def compose(head, face, body):
    g = [list(r) for r in BASE]
    for part in head.split():
        overlay(g, *HEAD[part])
    if FACE[face]:
        overlay(g, 6, FACE[face])
    for part in body.split():
        overlay(g, *BODY[part])
    return [''.join(r) for r in g]


def dex_names():
    """Species names straight from dex.h, so a label is never a second copy."""
    here = os.path.dirname(os.path.abspath(__file__))
    src = open(os.path.join(here, '..', 'dex.h'), encoding='utf-8').read()
    return {int(n): s for s, n in re.findall(r'\{ "([^"]+)",[^\n]*//\s*(\d+) ', src)}


def costumes():
    names = dex_names()
    out = []
    for dex, col, head, face, body in COSTUMES:
        label = names[dex][:10].rstrip()
        out.append((label, rgb565(col), compose(head, face, body)))
    return out
