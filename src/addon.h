#ifndef _addon_h_
#define _addon_h_

// Add-ons: blocks, items, recipes and creatures that someone else wrote.
//
// An add-on is **data, never code**. The browser reads a JSON file and calls
// the four register functions below; nothing an add-on says can run. It
// cannot reach the world, the inventory or the network -- it can only add
// definitions to the tables the game already has.
//
// Textures are not shipped either: an add-on gives a colour, and the tile is
// generated here and uploaded into a free slot of the atlas. That keeps an
// add-on a few hundred bytes and means no one can smuggle a file in.

void addon_set_atlas(unsigned int texture);
// implemented in main.c, which owns the OpenGL side
void addon_upload_tile(int tile, const unsigned char *pixels);
void addon_reset();

// All four return the new id, or 0 when nothing is free any more.
int addon_add_block(const char *name, int red, int green, int blue,
    int speckle, int transparent);
int addon_add_item(const char *name, int kind, int red, int green, int blue,
    int damage, int heal);
int addon_add_recipe(int result, int count,
    int item0, int count0, int item1, int count1);
int addon_add_creature(const char *name, int red, int green, int blue,
    int health, int speed, int damage, int hostile, int drop, int drop_count);

int addon_block_count();
int addon_item_count();

#endif
