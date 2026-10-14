#ifndef _container_h_
#define _container_h_

#include "inventory.h"

// A chest keeps things. One chest is open at a time, which is all a single
// pair of hands can manage anyway.
//
// The contents live in the world file, in the meta table, under the key
// "chest:x,y,z" -- one row per chest, written whenever something moves. No
// new table, so an older world keeps working, and each dimension keeps its
// own chests because each has its own file.

#define CHEST_SLOTS 27

void container_open(int x, int y, int z);
void container_close();
int container_is_open();

Slot *container_slot(int index);
int container_take(int index);        // chest -> bag, returns what moved
int container_put(int inventory_index);  // bag -> chest

// Everything a broken chest held goes to the player.
void container_break(int x, int y, int z);

#endif
