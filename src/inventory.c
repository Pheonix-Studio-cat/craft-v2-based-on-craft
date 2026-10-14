#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "inventory.h"
#include "item.h"

static void add_type(
    int id, const char *name, int kind, int block, int tile,
    int stack, int damage, int heal, float cooldown);
static void add_recipe(
    int result, int result_count,
    int item0, int count0, int item1, int count1, int item2, int count2);

static ItemType types[MAX_ITEM_TYPES];
static Slot inventory[INVENTORY_SIZE];
static Recipe recipes[MAX_RECIPES];
static int recipes_used = 0;
static int version = 1;

// ---------------------------------------------------------------------------
// the registry
// ---------------------------------------------------------------------------

// What an add-on registers goes through the same door the built-in items
// use -- there is only one table.
void inventory_register(
    int id, const char *name, int kind, int block, int tile,
    int stack, int damage, int heal, float cooldown)
{
    add_type(id, name, kind, block, tile, stack, damage, heal, cooldown);
}

int inventory_register_recipe(int result, int count,
    int item0, int count0, int item1, int count1)
{
    if (recipes_used >= MAX_RECIPES) {
        return 0;
    }
    add_recipe(result, count, item0, count0, item1, count1, 0, 0);
    return 1;
}

static void add_type(
    int id, const char *name, int kind, int block, int tile,
    int stack, int damage, int heal, float cooldown)
{
    if (id < 0 || id >= MAX_ITEM_TYPES) {
        return;
    }
    ItemType *type = types + id;
    type->id = id;
    snprintf(type->name, sizeof(type->name), "%s", name);
    type->kind = kind;
    type->block = block;
    type->tile = tile;
    type->stack = stack;
    type->damage = damage;
    type->heal = heal;
    type->cooldown = cooldown;
}

static void add_block_type(int block, const char *name) {
    // the icon is the block's own top texture
    add_type(block, name, KIND_BLOCK, block, blocks[block][2], 64, 1, 0, 0.25);
}

static void add_recipe(
    int result, int result_count,
    int item0, int count0, int item1, int count1, int item2, int count2)
{
    if (recipes_used >= MAX_RECIPES) {
        return;
    }
    Recipe *recipe = recipes + recipes_used++;
    recipe->result = result;
    recipe->result_count = result_count;
    recipe->item[0] = item0;
    recipe->count[0] = count0;
    recipe->item[1] = item1;
    recipe->count[1] = count1;
    recipe->item[2] = item2;
    recipe->count[2] = count2;
}

void inventory_init() {
    memset(types, 0, sizeof(types));
    recipes_used = 0;

    add_block_type(GRASS, "Grass");
    add_block_type(SAND, "Sand");
    add_block_type(STONE, "Stone");
    add_block_type(BRICK, "Brick");
    add_block_type(WOOD, "Wood");
    add_block_type(CEMENT, "Cement");
    add_block_type(DIRT, "Dirt");
    add_block_type(PLANK, "Plank");
    add_block_type(SNOW, "Snow");
    add_block_type(GLASS, "Glass");
    add_block_type(COBBLE, "Cobblestone");
    add_block_type(LIGHT_STONE, "Lamp");
    add_block_type(DARK_STONE, "Dark Stone");
    add_block_type(CHEST, "Chest");
    add_block_type(LEAVES, "Leaves");
    add_block_type(TALL_GRASS, "Tall Grass");
    add_block_type(YELLOW_FLOWER, "Yellow Flower");
    add_block_type(RED_FLOWER, "Red Flower");
    add_block_type(PURPLE_FLOWER, "Purple Flower");
    add_block_type(SUN_FLOWER, "Sun Flower");
    add_block_type(WHITE_FLOWER, "White Flower");
    add_block_type(BLUE_FLOWER, "Blue Flower");

    add_block_type(WATER, "Water");
    add_block_type(COAL_ORE, "Coal Ore");
    add_block_type(IRON_ORE, "Iron Ore");
    add_block_type(GEM_ORE, "Gem Ore");
    add_block_type(EMBER_ROCK, "Ember Rock");
    add_block_type(ASH_SAND, "Ash Sand");
    add_block_type(GLOW_ROCK, "Glow Rock");
    add_block_type(EMBER_BRICK, "Ember Brick");
    add_block_type(PORTAL_STONE, "Portal Stone");
    add_block_type(PORTAL, "Portal");
    add_block_type(LAVA, "Lava");

    for (int i = COLOR_00; i <= COLOR_31; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Colour %d", i - COLOR_00);
        add_block_type(i, name);
    }

    add_type(ITEM_STICK, "Stick", KIND_MATERIAL, 0, 96, 64, 1, 0, 0.25);
    add_type(ITEM_COAL, "Coal", KIND_MATERIAL, 0, 106, 64, 1, 0, 0.25);
    add_type(ITEM_IRON, "Iron Ingot", KIND_MATERIAL, 0, 107, 64, 1, 0, 0.25);
    add_type(ITEM_GEM, "Gem", KIND_MATERIAL, 0, 110, 64, 1, 0, 0.25);
    add_type(ITEM_EMBER_DUST, "Ember Dust", KIND_MATERIAL, 0, 108, 64, 1, 0, 0.25);
    add_type(ITEM_MEAT, "Cooked Meat", KIND_FOOD, 0, 109, 16, 1, 4, 0.8);
    add_type(ITEM_ARROW, "Bolt", KIND_MATERIAL, 0, 102, 64, 1, 0, 0.25);

    // Melee weapons. The numbers are the whole difference between them:
    // fast and light, or slow and heavy.
    add_type(ITEM_IRON_BLADE, "Iron Blade", KIND_MELEE, 0, 97, 1, 5, 0, 0.45);
    add_type(ITEM_GREAT_HAMMER, "Great Hammer", KIND_MELEE, 0, 98, 1, 9, 0, 1.10);
    add_type(ITEM_TWIN_SICKLES, "Twin Sickles", KIND_MELEE, 0, 99, 1, 3, 0, 0.22);
    add_type(ITEM_EMBER_BLADE, "Ember Blade", KIND_MELEE, 0, 112, 1, 7, 0, 0.40);

    add_type(ITEM_CROSSBOW, "Crossbow", KIND_RANGED, 0, 100, 1, 6, 0, 1.10);
    add_type(ITEM_RAPID_CROSSBOW, "Rapid Crossbow", KIND_RANGED, 0, 101, 1, 4, 0, 0.45);

    add_type(ITEM_PICKAXE, "Pickaxe", KIND_TOOL, 0, 103, 1, 2, 0, 0.30);
    add_type(ITEM_AXE, "Axe", KIND_TOOL, 0, 104, 1, 3, 0, 0.35);
    add_type(ITEM_SHOVEL, "Shovel", KIND_TOOL, 0, 105, 1, 2, 0, 0.30);
    add_type(ITEM_FIRE_STARTER, "Fire Starter", KIND_TOOL, 0, 111, 1, 1, 0, 0.50);

    add_recipe(PLANK, 4, WOOD, 1, 0, 0, 0, 0);
    add_recipe(ITEM_STICK, 4, PLANK, 2, 0, 0, 0, 0);
    add_recipe(ITEM_IRON, 1, IRON_ORE, 1, ITEM_COAL, 1, 0, 0);
    add_recipe(LIGHT_STONE, 4, ITEM_COAL, 1, STONE, 1, 0, 0);
    add_recipe(ITEM_PICKAXE, 1, ITEM_STICK, 2, COBBLE, 3, 0, 0);
    add_recipe(ITEM_AXE, 1, ITEM_STICK, 2, COBBLE, 3, 0, 0);
    add_recipe(ITEM_SHOVEL, 1, ITEM_STICK, 1, COBBLE, 1, 0, 0);
    add_recipe(ITEM_IRON_BLADE, 1, ITEM_IRON, 2, ITEM_STICK, 1, 0, 0);
    add_recipe(ITEM_TWIN_SICKLES, 1, ITEM_IRON, 2, ITEM_STICK, 2, 0, 0);
    add_recipe(ITEM_GREAT_HAMMER, 1, ITEM_IRON, 4, ITEM_STICK, 2, 0, 0);
    add_recipe(ITEM_CROSSBOW, 1, ITEM_IRON, 2, ITEM_STICK, 3, 0, 0);
    add_recipe(ITEM_RAPID_CROSSBOW, 1, ITEM_CROSSBOW, 1, ITEM_GEM, 1, 0, 0);
    add_recipe(ITEM_ARROW, 8, ITEM_STICK, 1, ITEM_IRON, 1, 0, 0);
    add_recipe(ITEM_FIRE_STARTER, 1, ITEM_IRON, 1, ITEM_COAL, 1, 0, 0);
    add_recipe(ITEM_EMBER_BLADE, 1, ITEM_IRON_BLADE, 1, ITEM_EMBER_DUST, 2, 0, 0);
    add_recipe(EMBER_BRICK, 4, EMBER_ROCK, 4, 0, 0, 0, 0);
    add_recipe(PORTAL_STONE, 1, EMBER_ROCK, 2, ITEM_GEM, 1, 0, 0);
}

ItemType *item_type(int id) {
    if (id <= 0 || id >= MAX_ITEM_TYPES || types[id].id == 0) {
        return 0;
    }
    return types + id;
}

int item_is_block(int id) {
    ItemType *type = item_type(id);
    return type && type->block;
}

// What breaking a block puts into the inventory. Most blocks give themselves.
int block_drop(int w) {
    switch (w) {
        case GRASS:
            return DIRT;
        case STONE:
            return COBBLE;
        case COAL_ORE:
            return ITEM_COAL;
        case IRON_ORE:
            return IRON_ORE;      // raw ore, smelted with coal
        case GEM_ORE:
            return ITEM_GEM;
        case EMBER_ROCK:
            return EMBER_ROCK;
        case GLOW_ROCK:
            return ITEM_EMBER_DUST;
        case CLOUD:
        case WATER:
        case LAVA:
        case PORTAL:
            return 0;
        default:
            return w;
    }
}

// ---------------------------------------------------------------------------
// the inventory itself
// ---------------------------------------------------------------------------

void inv_reset() {
    memset(inventory, 0, sizeof(inventory));
    version++;
}

void inv_give_starting_items(int game_mode) {
    inv_reset();
    if (game_mode == 0) {
        // creative: a hotbar full of the blocks people reach for first
        int start[HOTBAR_SIZE] = {
            GRASS, DIRT, STONE, COBBLE, WOOD, PLANK, GLASS, LIGHT_STONE, SAND
        };
        for (int i = 0; i < HOTBAR_SIZE; i++) {
            inv_set_slot(i, start[i], 1);
        }
    }
    else {
        // survival: nothing but the means to start
        inv_set_slot(0, ITEM_PICKAXE, 1);
        inv_set_slot(1, ITEM_IRON_BLADE, 1);
    }
}

Slot *inv_slot(int index) {
    if (index < 0 || index >= INVENTORY_SIZE) {
        return 0;
    }
    return inventory + index;
}

void inv_set_slot(int index, int item, int count) {
    Slot *slot = inv_slot(index);
    if (!slot) {
        return;
    }
    slot->item = count > 0 ? item : 0;
    slot->count = count > 0 ? count : 0;
    version++;
}

int inv_version() {
    return version;
}

// Something outside changed a slot -- say, a chest took from it.
void inv_touch_version() {
    version++;
}

// Returns what did not fit.
int inv_add(int item, int count) {
    ItemType *type = item_type(item);
    if (!type || count <= 0) {
        return count;
    }
    // fill partial stacks first, hotbar before storage
    for (int i = 0; i < INVENTORY_SIZE && count > 0; i++) {
        Slot *slot = inventory + i;
        if (slot->item == item && slot->count < type->stack) {
            int room = type->stack - slot->count;
            int take = count < room ? count : room;
            slot->count += take;
            count -= take;
        }
    }
    for (int i = 0; i < INVENTORY_SIZE && count > 0; i++) {
        Slot *slot = inventory + i;
        if (slot->count == 0) {
            int take = count < type->stack ? count : type->stack;
            slot->item = item;
            slot->count = take;
            count -= take;
        }
    }
    version++;
    return count;
}

int inv_count(int item) {
    int total = 0;
    for (int i = 0; i < INVENTORY_SIZE; i++) {
        if (inventory[i].item == item) {
            total += inventory[i].count;
        }
    }
    return total;
}

// Takes up to count, returns how many were actually taken.
int inv_take(int item, int count) {
    int taken = 0;
    for (int i = 0; i < INVENTORY_SIZE && taken < count; i++) {
        Slot *slot = inventory + i;
        if (slot->item != item) {
            continue;
        }
        int take = count - taken;
        if (take > slot->count) {
            take = slot->count;
        }
        slot->count -= take;
        taken += take;
        if (slot->count == 0) {
            slot->item = 0;
        }
    }
    if (taken) {
        version++;
    }
    return taken;
}

int inv_remove_at(int index, int count) {
    Slot *slot = inv_slot(index);
    if (!slot || slot->count == 0) {
        return 0;
    }
    int take = count < slot->count ? count : slot->count;
    slot->count -= take;
    if (slot->count == 0) {
        slot->item = 0;
    }
    version++;
    return take;
}

// Drag and drop from the inventory screen: merge when it is the same item,
// swap otherwise.
void inv_move(int from, int to) {
    Slot *a = inv_slot(from);
    Slot *b = inv_slot(to);
    if (!a || !b || from == to) {
        return;
    }
    ItemType *type = item_type(a->item);
    if (type && a->item == b->item && b->count < type->stack) {
        int room = type->stack - b->count;
        int take = a->count < room ? a->count : room;
        b->count += take;
        a->count -= take;
        if (a->count == 0) {
            a->item = 0;
        }
    }
    else {
        Slot copy = *a;
        *a = *b;
        *b = copy;
    }
    version++;
}

// ---------------------------------------------------------------------------
// crafting
// ---------------------------------------------------------------------------

int recipe_count() {
    return recipes_used;
}

Recipe *recipe_at(int index) {
    if (index < 0 || index >= recipes_used) {
        return 0;
    }
    return recipes + index;
}

int recipe_can_craft(int index) {
    Recipe *recipe = recipe_at(index);
    if (!recipe) {
        return 0;
    }
    for (int i = 0; i < MAX_INGREDIENTS; i++) {
        if (!recipe->item[i]) {
            continue;
        }
        if (inv_count(recipe->item[i]) < recipe->count[i]) {
            return 0;
        }
    }
    return 1;
}

int recipe_craft(int index) {
    if (!recipe_can_craft(index)) {
        return 0;
    }
    Recipe *recipe = recipe_at(index);
    for (int i = 0; i < MAX_INGREDIENTS; i++) {
        if (recipe->item[i]) {
            inv_take(recipe->item[i], recipe->count[i]);
        }
    }
    int left = inv_add(recipe->result, recipe->result_count);
    return recipe->result_count - left;
}

// ---------------------------------------------------------------------------
// what the browser interface reads
// ---------------------------------------------------------------------------

static char json_buffer[16384];

const char *items_json() {
    int n = 0;
    n += snprintf(json_buffer + n, sizeof(json_buffer) - n, "[");
    int first = 1;
    for (int id = 0; id < MAX_ITEM_TYPES; id++) {
        ItemType *type = types + id;
        if (type->id == 0) {
            continue;
        }
        n += snprintf(json_buffer + n, sizeof(json_buffer) - n,
            "%s{\"id\":%d,\"name\":\"%s\",\"kind\":%d,\"block\":%d,"
            "\"tile\":%d,\"stack\":%d,\"damage\":%d,\"heal\":%d}",
            first ? "" : ",", type->id, type->name, type->kind,
            type->block, type->tile, type->stack, type->damage, type->heal);
        first = 0;
        if (n > (int)sizeof(json_buffer) - 160) {
            break;
        }
    }
    snprintf(json_buffer + n, sizeof(json_buffer) - n, "]");
    return json_buffer;
}

static char recipe_buffer[4096];

const char *recipes_json() {
    int n = 0;
    n += snprintf(recipe_buffer + n, sizeof(recipe_buffer) - n, "[");
    for (int i = 0; i < recipes_used; i++) {
        Recipe *recipe = recipes + i;
        n += snprintf(recipe_buffer + n, sizeof(recipe_buffer) - n,
            "%s{\"result\":%d,\"count\":%d,\"need\":[",
            i ? "" : "", recipe->result, recipe->result_count);
        int first = 1;
        for (int j = 0; j < MAX_INGREDIENTS; j++) {
            if (!recipe->item[j]) {
                continue;
            }
            n += snprintf(recipe_buffer + n, sizeof(recipe_buffer) - n,
                "%s[%d,%d]", first ? "" : ",", recipe->item[j], recipe->count[j]);
            first = 0;
        }
        n += snprintf(recipe_buffer + n, sizeof(recipe_buffer) - n,
            "]}%s", i + 1 < recipes_used ? "," : "");
        if (n > (int)sizeof(recipe_buffer) - 160) {
            break;
        }
    }
    snprintf(recipe_buffer + n, sizeof(recipe_buffer) - n, "]");
    return recipe_buffer;
}

// ---------------------------------------------------------------------------
// saving
// ---------------------------------------------------------------------------

static char save_buffer[2048];

const char *inventory_save_string() {
    int n = 0;
    for (int i = 0; i < INVENTORY_SIZE; i++) {
        n += snprintf(save_buffer + n, sizeof(save_buffer) - n,
            "%s%d:%d", i ? "," : "", inventory[i].item, inventory[i].count);
        if (n > (int)sizeof(save_buffer) - 32) {
            break;
        }
    }
    return save_buffer;
}

void inventory_load_string(const char *text) {
    if (!text || !*text) {
        return;
    }
    inv_reset();
    const char *p = text;
    for (int i = 0; i < INVENTORY_SIZE && p && *p; i++) {
        int item = 0;
        int count = 0;
        if (sscanf(p, "%d:%d", &item, &count) != 2) {
            break;
        }
        if (count > 0 && item_type(item)) {
            inventory[i].item = item;
            inventory[i].count = count;
        }
        p = strchr(p, ',');
        if (p) {
            p++;
        }
    }
    version++;
}
