#ifndef _inventory_h_
#define _inventory_h_

// Items, the inventory and the crafting list.
//
// An item that is a block keeps the block's own id, so the world and the
// inventory speak the same language. Everything that is not a block -- tools,
// weapons, materials -- starts at 128.

#define MAX_ITEM_TYPES 256

#define KIND_BLOCK 0
#define KIND_TOOL 1
#define KIND_MELEE 2
#define KIND_RANGED 3
#define KIND_MATERIAL 4
#define KIND_FOOD 5

// non-block items
#define ITEM_STICK 128
#define ITEM_COAL 129
#define ITEM_IRON 130
#define ITEM_GEM 131
#define ITEM_EMBER_DUST 132
#define ITEM_MEAT 133
#define ITEM_ARROW 134
#define ITEM_IRON_BLADE 140
#define ITEM_GREAT_HAMMER 141
#define ITEM_TWIN_SICKLES 142
#define ITEM_EMBER_BLADE 143
#define ITEM_CROSSBOW 150
#define ITEM_RAPID_CROSSBOW 151
#define ITEM_PICKAXE 160
#define ITEM_AXE 161
#define ITEM_SHOVEL 162
#define ITEM_FIRE_STARTER 163

#define HOTBAR_SIZE 9
#define INVENTORY_SIZE 36

#define MAX_RECIPES 32
#define MAX_INGREDIENTS 3

typedef struct {
    int id;
    char name[32];
    int kind;
    int block;        // block id when placeable, 0 otherwise
    int tile;         // atlas tile used as icon and as held model
    int stack;
    int damage;       // melee damage, or damage per bolt for a crossbow
    int heal;         // hit points a food item restores
    float cooldown;   // seconds between two uses
} ItemType;

typedef struct {
    int item;
    int count;
} Slot;

typedef struct {
    int result;
    int result_count;
    int item[MAX_INGREDIENTS];
    int count[MAX_INGREDIENTS];
} Recipe;

void inventory_init();
void inventory_register(
    int id, const char *name, int kind, int block, int tile,
    int stack, int damage, int heal, float cooldown);
int inventory_register_recipe(int result, int count,
    int item0, int count0, int item1, int count1);
ItemType *item_type(int id);
int item_is_block(int id);
int block_drop(int w);

void inv_reset();
void inv_give_starting_items(int game_mode);
Slot *inv_slot(int index);
int inv_add(int item, int count);
int inv_take(int item, int count);
int inv_count(int item);
int inv_remove_at(int index, int count);
void inv_move(int from, int to);
void inv_set_slot(int index, int item, int count);
int inv_version();
void inv_touch_version();

int recipe_count();
Recipe *recipe_at(int index);
int recipe_can_craft(int index);
int recipe_craft(int index);

// JSON for the browser interface. Both return a pointer to a static buffer.
const char *items_json();
const char *recipes_json();

// compact "item:count,item:count,..." string for the database
const char *inventory_save_string();
void inventory_load_string(const char *text);

#endif
