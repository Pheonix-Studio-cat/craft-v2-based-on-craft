#include <stdlib.h>
#include <string.h>
#include "addon.h"
#include "entity.h"
#include "inventory.h"
#include "item.h"
#include "util.h"

// where add-on things are allowed to live, so they never collide with the
// game's own numbers
#define ADDON_FIRST_BLOCK 67
#define ADDON_LAST_BLOCK 127
#define ADDON_FIRST_ITEM 200
#define ADDON_LAST_ITEM 250
#define ADDON_FIRST_TILE 113
#define ADDON_LAST_TILE 159

static unsigned int atlas = 0;
static int next_block = ADDON_FIRST_BLOCK;
static int next_item = ADDON_FIRST_ITEM;
static int next_tile = ADDON_FIRST_TILE;

void addon_set_atlas(unsigned int texture) {
    atlas = texture;
}

void addon_reset() {
    next_block = ADDON_FIRST_BLOCK;
    next_item = ADDON_FIRST_ITEM;
    next_tile = ADDON_FIRST_TILE;
}

int addon_block_count() {
    return next_block - ADDON_FIRST_BLOCK;
}

int addon_item_count() {
    return next_item - ADDON_FIRST_ITEM;
}

// A tile out of one colour: a little noise, a darker bottom edge, so it does
// not look like a flat sticker between the hand-drawn blocks.
static int make_tile(int red, int green, int blue, int speckle, int icon) {
    if (next_tile > ADDON_LAST_TILE || !atlas) {
        return 0;
    }
    int tile = next_tile++;
    unsigned char pixels[16 * 16 * 4];
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            int i = (y * 16 + x) * 4;
            int inside = 1;
            if (icon) {
                // an icon is a rounded lump, the rest is the colour the
                // shader throws away
                int dx = x - 8;
                int dy = y - 8;
                inside = dx * dx + dy * dy < 36;
            }
            if (!inside) {
                pixels[i + 0] = 255;
                pixels[i + 1] = 0;
                pixels[i + 2] = 255;
                pixels[i + 3] = 255;
                continue;
            }
            int noise = speckle ? (rand_int(2 * speckle + 1) - speckle) : 0;
            int shade = y > 12 ? -18 : (y < 2 ? 12 : 0);
            int values[3] = {red + noise + shade, green + noise + shade,
                             blue + noise + shade};
            for (int c = 0; c < 3; c++) {
                int v = values[c];
                pixels[i + c] = v < 0 ? 0 : (v > 255 ? 255 : v);
            }
            pixels[i + 3] = 255;
        }
    }
    addon_upload_tile(tile, pixels);
    return tile;
}

int addon_add_block(const char *name, int red, int green, int blue,
    int speckle, int transparent)
{
    if (next_block > ADDON_LAST_BLOCK) {
        return 0;
    }
    int tile = make_tile(red, green, blue, speckle ? speckle : 8, 0);
    if (!tile) {
        return 0;
    }
    int id = next_block++;
    for (int face = 0; face < 6; face++) {
        blocks[id][face] = tile;
    }
    item_register_block(id, transparent);
    inventory_register(id, name, KIND_BLOCK, id, tile, 64, 1, 0, 0.25);
    return id;
}

int addon_add_item(const char *name, int kind, int red, int green, int blue,
    int damage, int heal)
{
    if (next_item > ADDON_LAST_ITEM) {
        return 0;
    }
    int tile = make_tile(red, green, blue, 6, 1);
    if (!tile) {
        return 0;
    }
    int id = next_item++;
    int stack = (kind == KIND_MELEE || kind == KIND_RANGED) ? 1 : 64;
    float cooldown = kind == KIND_RANGED ? 0.9 : 0.45;
    inventory_register(id, name, kind, 0, tile, stack, damage, heal, cooldown);
    return id;
}

int addon_add_recipe(int result, int count,
    int item0, int count0, int item1, int count1)
{
    if (!item_type(result)) {
        return 0;
    }
    return inventory_register_recipe(result, count, item0, count0,
        item1, count1);
}

int addon_add_creature(const char *name, int red, int green, int blue,
    int health, int speed, int damage, int hostile, int drop, int drop_count)
{
    int tile = make_tile(red, green, blue, 10, 0);
    if (!tile) {
        return 0;
    }
    return entity_register_type(name, tile, health, speed / 10.0, damage,
        hostile, drop, drop_count);
}
