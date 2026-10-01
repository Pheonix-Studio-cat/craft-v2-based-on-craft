#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "container.h"
#include "db.h"
#include "inventory.h"

static Slot slots[CHEST_SLOTS];
static int open_x, open_y, open_z;
static int is_open = 0;

static void key_for(int x, int y, int z, char *buffer, int length) {
    snprintf(buffer, length, "chest:%d,%d,%d", x, y, z);
}

static void load(int x, int y, int z) {
    memset(slots, 0, sizeof(slots));
    char key[64];
    char value[1024];
    key_for(x, y, z, key, sizeof(key));
    if (!db_get_meta(key, value, sizeof(value))) {
        return;
    }
    const char *p = value;
    for (int i = 0; i < CHEST_SLOTS && p && *p; i++) {
        int item = 0;
        int count = 0;
        if (sscanf(p, "%d:%d", &item, &count) != 2) {
            break;
        }
        if (count > 0 && item_type(item)) {
            slots[i].item = item;
            slots[i].count = count;
        }
        p = strchr(p, ',');
        if (p) {
            p++;
        }
    }
}

static void store() {
    if (!is_open) {
        return;
    }
    char key[64];
    char value[1024];
    key_for(open_x, open_y, open_z, key, sizeof(key));
    int n = 0;
    int empty = 1;
    for (int i = 0; i < CHEST_SLOTS; i++) {
        n += snprintf(value + n, sizeof(value) - n, "%s%d:%d",
            i ? "," : "", slots[i].item, slots[i].count);
        if (slots[i].count) {
            empty = 0;
        }
        if (n > (int)sizeof(value) - 32) {
            break;
        }
    }
    db_set_meta(key, empty ? "" : value);
}

void container_open(int x, int y, int z) {
    if (is_open) {
        store();
    }
    open_x = x;
    open_y = y;
    open_z = z;
    is_open = 1;
    load(x, y, z);
}

void container_close() {
    if (!is_open) {
        return;
    }
    store();
    is_open = 0;
}

int container_is_open() {
    return is_open;
}

Slot *container_slot(int index) {
    if (index < 0 || index >= CHEST_SLOTS) {
        return 0;
    }
    return slots + index;
}

int container_take(int index) {
    Slot *slot = container_slot(index);
    if (!is_open || !slot || !slot->count) {
        return 0;     // no chest open: the slots below belong to no one
    }
    int left = inv_add(slot->item, slot->count);
    int moved = slot->count - left;
    slot->count = left;
    if (!slot->count) {
        slot->item = 0;
    }
    store();
    return moved;
}

int container_put(int inventory_index) {
    Slot *from = inv_slot(inventory_index);
    if (!is_open || !from || !from->count) {
        return 0;
    }
    ItemType *type = item_type(from->item);
    int moved = 0;
    // partial stacks first, then an empty slot -- the same order the bag uses
    for (int pass = 0; pass < 2 && from->count; pass++) {
        for (int i = 0; i < CHEST_SLOTS && from->count; i++) {
            Slot *slot = slots + i;
            if (pass == 0 && slot->item != from->item) {
                continue;
            }
            if (pass == 1 && slot->count) {
                continue;
            }
            int room = (pass == 0 ? type->stack - slot->count : type->stack);
            if (room <= 0) {
                continue;
            }
            int take = from->count < room ? from->count : room;
            slot->item = from->item;
            slot->count += take;
            from->count -= take;
            moved += take;
        }
    }
    if (!from->count) {
        from->item = 0;
    }
    if (moved) {
        inv_touch_version();
        store();
    }
    return moved;
}

// Breaking a chest hands its contents to the player instead of swallowing
// them. What no longer fits stays lost -- the bag has a size, and a message
// says so at the call site.
void container_break(int x, int y, int z) {
    int was_open = is_open;
    int old_x = open_x;
    int old_y = open_y;
    int old_z = open_z;

    container_open(x, y, z);
    for (int i = 0; i < CHEST_SLOTS; i++) {
        if (slots[i].count) {
            inv_add(slots[i].item, slots[i].count);
            slots[i].item = 0;
            slots[i].count = 0;
        }
    }
    store();
    is_open = 0;

    if (was_open && (old_x != x || old_y != y || old_z != z)) {
        container_open(old_x, old_y, old_z);
    }
}
