#!/usr/bin/env python3
"""Bake the tiles this fork adds into textures/texture.png.

The atlas is 16x16 tiles of 16x16 pixels. Craft flips the image vertically
when it loads it, so tile row `t // 16` counts from the BOTTOM of the file.
Tiles 0-54 and 160-231 belong to upstream Craft and are never touched here;
everything this script writes lives at 64..127, which upstream leaves empty
(magenta, the colour the block shader discards).

Run it from the repository root after changing a pattern:

    python3 tools/make_textures.py

It is deterministic: the same script always produces the same pixels.
"""
import random
from PIL import Image

ATLAS = 'textures/texture.png'
SIZE = 16

# --------------------------------------------------------------------------
# helpers
# --------------------------------------------------------------------------

def noisy(base, spread, seed, shades=None):
    """A flat 16x16 tile with per-pixel noise, the look of upstream's stone."""
    rng = random.Random(seed)
    tile = Image.new('RGBA', (SIZE, SIZE))
    px = tile.load()
    for y in range(SIZE):
        for x in range(SIZE):
            if shades and rng.random() < 0.12:
                c = shades[rng.randrange(len(shades))]
            else:
                d = rng.randint(-spread, spread)
                c = tuple(max(0, min(255, v + d)) for v in base)
            px[x, y] = (c[0], c[1], c[2], 255)
    return tile


def speckle(tile, colour, seed, blobs=6, size=2):
    """Scatter small blobs -- ore in stone, embers in rock."""
    rng = random.Random(seed)
    px = tile.load()
    for _ in range(blobs):
        bx, by = rng.randrange(SIZE - size), rng.randrange(SIZE - size)
        for dy in range(size + rng.randrange(2)):
            for dx in range(size + rng.randrange(2)):
                x, y = (bx + dx) % SIZE, (by + dy) % SIZE
                d = rng.randint(-14, 14)
                px[x, y] = tuple(max(0, min(255, v + d)) for v in colour) + (255,)
    return tile


MAGENTA = (255, 0, 255, 255)


def from_map(rows, palette):
    """Pixel art from a 16-line string map.

    Empty pixels are MAGENTA, not transparent: the block shader discards
    exactly that colour, while alpha is ignored when a tile is drawn in the
    world. The inventory in the browser strips the same colour again.
    """
    tile = Image.new('RGBA', (SIZE, SIZE), MAGENTA)
    px = tile.load()
    for y, row in enumerate(rows):
        for x, ch in enumerate(row[:SIZE]):
            if ch in ' .':
                continue
            px[x, y] = palette[ch] + (255,)
    return tile


# --------------------------------------------------------------------------
# blocks: 64..79
# --------------------------------------------------------------------------

def water():
    rng = random.Random(4711)
    tile = Image.new('RGBA', (SIZE, SIZE))
    px = tile.load()
    for y in range(SIZE):
        for x in range(SIZE):
            # gentle horizontal banding, so the wave in the shader has
            # something to move
            band = 8 * (1 + (x * 3 + y * 5) % 3)
            d = rng.randint(-6, 6)
            px[x, y] = (30 + d, 90 + band // 2 + d, 190 + band + d, 255)
    return tile


def lava():
    rng = random.Random(1312)
    tile = Image.new('RGBA', (SIZE, SIZE))
    px = tile.load()
    for y in range(SIZE):
        for x in range(SIZE):
            d = rng.randint(-18, 18)
            hot = 40 if (x + y * 2) % 7 < 2 else 0
            px[x, y] = (210 + hot // 3 + d, 70 + hot + d // 2, 20 + d // 3, 255)
    return tile


def portal_surface():
    rng = random.Random(90210)
    tile = Image.new('RGBA', (SIZE, SIZE))
    px = tile.load()
    for y in range(SIZE):
        for x in range(SIZE):
            swirl = int(28 * ((x * y + x * 3) % 5) / 4)
            d = rng.randint(-12, 12)
            px[x, y] = (110 + swirl + d, 40 + swirl // 2 + d, 170 + swirl + d, 255)
    return tile


BLOCK_TILES = {
    64: lambda: water(),
    65: lambda: speckle(noisy((122, 122, 122), 10, 65), (32, 32, 36), 65, 7, 2),      # coal ore
    66: lambda: speckle(noisy((122, 122, 122), 10, 66), (198, 160, 120), 66, 6, 2),   # iron ore
    67: lambda: speckle(noisy((116, 46, 44), 14, 67), (86, 30, 30), 67, 10, 2),       # ember rock
    68: lambda: speckle(noisy((78, 60, 58), 10, 68), (52, 40, 38), 68, 9, 3),         # ash sand
    69: lambda: speckle(noisy((214, 176, 92), 12, 69), (250, 224, 150), 69, 8, 2),    # glow rock
    70: lambda: ember_brick(),                                                        # ember brick
    71: lambda: noisy((36, 28, 46), 7, 71, shades=[(24, 18, 32), (50, 40, 62)]),      # portal stone
    72: lambda: portal_surface(),
    73: lambda: lava(),
    74: lambda: speckle(noisy((150, 150, 158), 9, 74), (96, 200, 210), 74, 5, 2),     # gem ore
}


def ember_brick():
    tile = noisy((72, 26, 30), 8, 70)
    px = tile.load()
    mortar = (44, 16, 20)
    for y in range(SIZE):
        for x in range(SIZE):
            if y % 8 == 0 or (x + (0 if (y // 8) % 2 == 0 else 8)) % 16 == 0:
                px[x, y] = mortar + (255,)
    return tile


# --------------------------------------------------------------------------
# creatures: 80..95  (one tile per creature, used for body and head)
# --------------------------------------------------------------------------

MOB_PALETTES = {
    80: ((92, 140, 78), (58, 96, 52), (28, 40, 26)),      # husk, green
    81: ((196, 196, 200), (150, 150, 156), (40, 40, 44)),  # bonewalker, white
    82: ((60, 52, 78), (40, 34, 54), (180, 60, 60)),       # creeper-ish stalker
    83: ((128, 96, 64), (96, 70, 46), (30, 24, 18)),       # boar
    84: ((210, 205, 195), (170, 165, 158), (60, 58, 54)),  # sheep-ish woolly
    85: ((88, 196, 120), (60, 150, 92), (24, 60, 40)),     # slime
    86: ((54, 48, 60), (34, 30, 40), (200, 40, 40)),       # cave spider
    87: ((186, 92, 56), (140, 66, 40), (40, 20, 14)),      # ember piglet (nether)
    88: ((150, 40, 40), (100, 26, 26), (250, 190, 90)),    # magma brute (nether)
    89: ((60, 60, 72), (40, 40, 50), (160, 220, 255)),     # wraith (nether)
}


def mob_tile(index):
    body, dark, eye = MOB_PALETTES[index]
    tile = noisy(body, 10, index * 7 + 1)
    px = tile.load()
    # darker belt at the bottom, two eyes near the top -- enough for a face on
    # the head cube and a pattern on the body cube
    for y in range(11, SIZE):
        for x in range(SIZE):
            r, g, b, a = px[x, y]
            px[x, y] = (dark[0], dark[1], dark[2], 255) if (x + y) % 3 else (r, g, b, a)
    for ex in (3, 4, 10, 11):
        for ey in (5, 6):
            px[ex, ey] = eye + (255,)
    return tile


# --------------------------------------------------------------------------
# item icons: 96..119
# --------------------------------------------------------------------------

P = {
    'w': (110, 78, 48),    # wood
    'W': (78, 54, 32),     # wood dark
    's': (198, 200, 210),  # steel
    'S': (140, 144, 156),  # steel dark
    'g': (226, 190, 92),   # gold
    'k': (40, 40, 46),     # black
    'r': (170, 60, 50),    # red
    'b': (120, 100, 200),  # violet
    'c': (110, 200, 210),  # gem
    'f': (232, 216, 180),  # bone / string
    'm': (150, 80, 60),    # meat
    'o': (232, 160, 60),   # ember
}

ICONS = {
    96: ("stick", [
        "                ",
        "                ",
        "             w  ",
        "            ww  ",
        "           ww   ",
        "          ww    ",
        "         ww     ",
        "        ww      ",
        "       ww       ",
        "      ww        ",
        "     ww         ",
        "    ww          ",
        "   ww           ",
        "  ww            ",
        "                ",
        "                ",
    ]),
    97: ("iron blade", [
        "             ss ",
        "            sss ",
        "           sssS ",
        "          sssS  ",
        "         sssS   ",
        "        sssS    ",
        "       sssS     ",
        "      sssS      ",
        "     sssS       ",
        "    sssS        ",
        "   wSwS         ",
        "  wwwww         ",
        "   wWw          ",
        "  ww w          ",
        " ww             ",
        "                ",
    ]),
    98: ("great hammer", [
        "                ",
        "      ssssss    ",
        "     sSSSSSSs   ",
        "    ssSssssSs   ",
        "    ssSssssSs   ",
        "     sSSSSSSs   ",
        "      ssssss    ",
        "       ww       ",
        "       ww       ",
        "      ww        ",
        "      ww        ",
        "     ww         ",
        "     ww         ",
        "    WW          ",
        "    WW          ",
        "                ",
    ]),
    99: ("twin sickles", [
        "                ",
        "   sss    sss   ",
        "  sS Ss  sS Ss  ",
        " sS   S  S   Ss ",
        " s    S  S    s ",
        "      S  S      ",
        "      S  S      ",
        "     ww  ww     ",
        "     ww  ww     ",
        "     wW  Ww     ",
        "     ww  ww     ",
        "                ",
        "                ",
        "                ",
        "                ",
        "                ",
    ]),
    100: ("crossbow", [
        "                ",
        "  ff            ",
        "  wff           ",
        "  w ff          ",
        "  w  ffff       ",
        "  w     fff     ",
        "  wwwwwwwwww    ",
        "  w  WW    ww   ",
        "  w   WW        ",
        "  w  ffff       ",
        "  w ff          ",
        "  wff           ",
        "  ff            ",
        "                ",
        "                ",
        "                ",
    ]),
    101: ("rapid crossbow", [
        "                ",
        "  ff            ",
        "  gff           ",
        "  g ff          ",
        "  g  ffff       ",
        "  g     fff     ",
        "  gggggggggg    ",
        "  g  ss    gg   ",
        "  g   ss        ",
        "  g  ffff       ",
        "  g ff          ",
        "  gff           ",
        "  ff            ",
        "                ",
        "                ",
        "                ",
    ]),
    102: ("arrow", [
        "                ",
        "             ss ",
        "            sss ",
        "           sSs  ",
        "          ww    ",
        "         ww     ",
        "        ww      ",
        "       ww       ",
        "      ww        ",
        "     ww         ",
        "    ww          ",
        "   fw           ",
        "  ff            ",
        " f f            ",
        "  ff            ",
        "                ",
    ]),
    103: ("pickaxe", [
        "                ",
        "   sss    sss   ",
        "  sSSSssssSSSs  ",
        "  s   ssss   s  ",
        "        ww      ",
        "       ww       ",
        "       ww       ",
        "      ww        ",
        "      ww        ",
        "     ww         ",
        "     ww         ",
        "    WW          ",
        "    WW          ",
        "   WW           ",
        "                ",
        "                ",
    ]),
    104: ("axe", [
        "                ",
        "     ssss       ",
        "    sSSSss      ",
        "    sSSSSs      ",
        "    sSSSs       ",
        "     sww        ",
        "      ww        ",
        "      ww        ",
        "     ww         ",
        "     ww         ",
        "    ww          ",
        "    WW          ",
        "   WW           ",
        "   WW           ",
        "                ",
        "                ",
    ]),
    105: ("shovel", [
        "                ",
        "      sss       ",
        "     sSSSs      ",
        "     sSSSs      ",
        "      sss       ",
        "      www       ",
        "      ww        ",
        "      ww        ",
        "     ww         ",
        "     ww         ",
        "    ww          ",
        "    WW          ",
        "   WW           ",
        "   WW           ",
        "                ",
        "                ",
    ]),
    106: ("coal", [
        "                ",
        "                ",
        "     kkkk       ",
        "    kkkkkk      ",
        "   kkkkkkkk     ",
        "   kkkkkkkk     ",
        "   kkkkkkkkk    ",
        "   kkkkkkkk     ",
        "    kkkkkkk     ",
        "     kkkkk      ",
        "      kkk       ",
        "                ",
        "                ",
        "                ",
        "                ",
        "                ",
    ]),
    107: ("iron ingot", [
        "                ",
        "                ",
        "                ",
        "                ",
        "      ssss      ",
        "     ssssss     ",
        "    ssSSSSss    ",
        "    sSSSSSSs    ",
        "    SSSSSSSS    ",
        "     SSSSSS     ",
        "                ",
        "                ",
        "                ",
        "                ",
        "                ",
        "                ",
    ]),
    108: ("ember dust", [
        "                ",
        "                ",
        "       o        ",
        "      o o       ",
        "     ooooo      ",
        "    oo o oo     ",
        "   ooooooooo    ",
        "    oo o oo     ",
        "     ooooo      ",
        "      o o       ",
        "       o        ",
        "                ",
        "                ",
        "                ",
        "                ",
        "                ",
    ]),
    109: ("cooked meat", [
        "                ",
        "                ",
        "      ffff      ",
        "     fmmmmf     ",
        "    fmmmmmmf    ",
        "    mmmmmmmm    ",
        "    mmmmmmmm    ",
        "    fmmmmmmf    ",
        "     fmmmmf     ",
        "      f  f      ",
        "      f  f      ",
        "                ",
        "                ",
        "                ",
        "                ",
        "                ",
    ]),
    110: ("gem", [
        "                ",
        "                ",
        "      cccc      ",
        "     cccccc     ",
        "    cccccccc    ",
        "    cccccccc    ",
        "     cccccc     ",
        "      cccc      ",
        "       cc       ",
        "                ",
        "                ",
        "                ",
        "                ",
        "                ",
        "                ",
        "                ",
    ]),
    111: ("fire starter", [
        "                ",
        "        ss      ",
        "       ssS      ",
        "      ssS       ",
        "     ssS        ",
        "    ssS         ",
        "   sS           ",
        "  ww            ",
        "  wW  o         ",
        "  ww oo o       ",
        "     ooooo      ",
        "      ooo       ",
        "                ",
        "                ",
        "                ",
        "                ",
    ]),
    112: ("ember blade", [
        "             oo ",
        "            ooo ",
        "           oooS ",
        "          oooS  ",
        "         oooS   ",
        "        oooS    ",
        "       oooS     ",
        "      oooS      ",
        "     oooS       ",
        "    oooS        ",
        "   kSkS         ",
        "  kkkkk         ",
        "   kWk          ",
        "  kk k          ",
        " kk             ",
        "                ",
    ]),
}


def main():
    atlas = Image.open(ATLAS).convert('RGBA')
    height = atlas.size[1]

    def put(index, tile):
        col, row = index % 16, index // 16
        atlas.paste(tile, (col * SIZE, height - (row + 1) * SIZE))

    for index, make in BLOCK_TILES.items():
        put(index, make())
    for index in MOB_PALETTES:
        put(index, mob_tile(index))
    for index, (_name, rows) in ICONS.items():
        put(index, from_map(rows, P))

    atlas.save(ATLAS)
    written = sorted(list(BLOCK_TILES) + list(MOB_PALETTES) + list(ICONS))
    print('wrote %d tiles: %s' % (len(written), written))


if __name__ == '__main__':
    main()
