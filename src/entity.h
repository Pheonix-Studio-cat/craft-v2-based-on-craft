#ifndef _entity_h_
#define _entity_h_

// Creatures and bolts.
//
// The list lives here, the world does not: everything this file needs to
// know about blocks it asks through the three functions at the bottom, which
// main.c provides. That keeps the creature code readable and testable.

#define MAX_ENTITIES 48
#define MAX_PROJECTILES 24

#define MOB_HUSK 0
#define MOB_BONEWALKER 1
#define MOB_STALKER 2
#define MOB_BOAR 3
#define MOB_WOOLLY 4
#define MOB_SLIME 5
#define MOB_SPIDER 6
#define MOB_EMBER_PIGLET 7
#define MOB_MAGMA_BRUTE 8
#define MOB_WRAITH 9
#define MOB_KIND_COUNT 10

typedef struct {
    char name[24];
    int tile;          // atlas tile for body and head
    float width;       // half width in blocks
    float height;      // total height in blocks
    float health;
    float speed;       // blocks per second
    float damage;
    float sight;       // how far it notices the player
    float attack_rate; // seconds between two hits
    int hostile;
    int night_only;    // burns off at daylight: despawns in the sun
    int hops;          // moves in jumps, like a slime
    int drop_item;
    int drop_count;
    int dimension;     // 0 overworld, 1 underworld
} MobType;

typedef struct {
    int used;
    int kind;
    float x, y, z;
    float rx;              // where it looks
    float dy;              // vertical speed
    float health;
    float attack_timer;
    float wander_timer;
    float wander_x, wander_z;
    float hurt_flash;      // seconds left of the white flash after a hit
} Entity;

typedef struct {
    int used;
    float x, y, z;
    float vx, vy, vz;
    float life;
    float damage;
    int hit;           // creature index + 1 when it struck one this frame
} Projectile;

void entities_init();
void entities_clear();
int entities_tick_count();
MobType *mob_type(int kind);
Entity *entity_at(int index);
int entity_count();
Projectile *projectile_at(int index);

int entity_spawn(int kind, float x, float y, float z);
void entity_spawn_in_front(int kind, float x, float y, float z, float rx);
void projectile_spawn(
    float x, float y, float z, float vx, float vy, float vz, float damage);

// One tick of everything alive. Returns the damage the player takes.
float entities_update(
    double dt, float px, float py, float pz, int dimension,
    float daylight, int paused);

// Nature keeps filling the world back up around the player.
void entities_maybe_spawn(
    double dt, float px, float py, float pz, int dimension, float daylight);

// The creature in the crosshair, or -1. Also used by the bolts.
int entity_pick(
    float x, float y, float z, float vx, float vy, float vz, float range);
int entity_pick_near(
    float x, float y, float z, float vx, float vz, float radius);

// Returns the item the creature dropped, 0 when it survived.
int entity_damage(int index, float amount, int *drop_count);

#endif
