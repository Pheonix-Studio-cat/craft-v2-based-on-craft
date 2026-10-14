#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "entity.h"
#include "inventory.h"
#include "item.h"
#include "util.h"

// provided by main.c -- the only thing the creatures need from the world
int entity_block_at(int x, int y, int z);

static MobType types[MAX_MOB_TYPES];
static int type_count = MOB_KIND_COUNT;
static Entity entities[MAX_ENTITIES];
static Projectile projectiles[MAX_PROJECTILES];
static double spawn_timer = 0;
static int tick_count = 0;   // read through craft_probe(7)

static void add_type(
    int kind, const char *name, int tile, float width, float height,
    float health, float speed, float damage, float sight, float attack_rate,
    int hostile, int night_only, int hops, int drop_item, int drop_count,
    int dimension)
{
    MobType *type = types + kind;
    snprintf(type->name, sizeof(type->name), "%s", name);
    type->tile = tile;
    type->width = width;
    type->height = height;
    type->health = health;
    type->speed = speed;
    type->damage = damage;
    type->sight = sight;
    type->attack_rate = attack_rate;
    type->hostile = hostile;
    type->night_only = night_only;
    type->hops = hops;
    type->drop_item = drop_item;
    type->drop_count = drop_count;
    type->dimension = dimension;
}

void entities_init() {
    memset(types, 0, sizeof(types));
    type_count = MOB_KIND_COUNT;
    //       kind            name          tile  w     h    hp   spd  dmg  see  rate  host night hop drop            n  dim
    add_type(MOB_HUSK,       "Husk",        80, 0.4, 1.9, 20, 2.6, 2.5, 26, 1.2, 1, 1, 0, ITEM_COAL,      1, 0);
    add_type(MOB_BONEWALKER, "Bonewalker",  81, 0.4, 1.9, 16, 2.9, 2.0, 30, 1.4, 1, 1, 0, ITEM_ARROW,     2, 0);
    add_type(MOB_STALKER,    "Stalker",     82, 0.4, 1.7, 14, 3.3, 3.5, 24, 1.6, 1, 0, 0, ITEM_COAL,      2, 0);
    add_type(MOB_BOAR,       "Boar",        83, 0.5, 1.2, 10, 1.6, 0.0, 10, 2.0, 0, 0, 0, ITEM_MEAT,      2, 0);
    add_type(MOB_WOOLLY,     "Woolly",      84, 0.5, 1.3, 10, 1.4, 0.0, 10, 2.0, 0, 0, 0, ITEM_MEAT,      1, 0);
    add_type(MOB_SLIME,      "Slime",       85, 0.5, 1.0, 12, 2.2, 1.5, 18, 1.4, 1, 0, 1, ITEM_GEM,       1, 0);
    add_type(MOB_SPIDER,     "Cave Spider", 86, 0.6, 0.9, 12, 3.6, 1.5, 22, 1.0, 1, 1, 0, ITEM_STICK,     1, 0);
    add_type(MOB_EMBER_PIGLET, "Ember Piglet", 87, 0.4, 1.5, 14, 2.8, 2.5, 22, 1.2, 1, 0, 0, ITEM_EMBER_DUST, 1, 1);
    add_type(MOB_MAGMA_BRUTE,  "Magma Brute", 88, 0.6, 2.2, 30, 2.2, 5.0, 26, 1.8, 1, 0, 0, ITEM_EMBER_DUST, 3, 1);
    add_type(MOB_WRAITH,       "Wraith",      89, 0.4, 1.8, 12, 3.4, 3.0, 30, 1.2, 1, 0, 0, ITEM_GEM,        1, 1);
    entities_clear();
}

int entities_tick_count() {
    return tick_count;
}

void entities_clear() {
    memset(entities, 0, sizeof(entities));
    memset(projectiles, 0, sizeof(projectiles));
    // a cleared world stays clear for a moment, instead of refilling in the
    // very next frame
    spawn_timer = 2.5;
}

MobType *mob_type(int kind) {
    if (kind < 0 || kind >= type_count) {
        return 0;
    }
    return types + kind;
}

int mob_type_count() {
    return type_count;
}

// An add-on creature: a colour, a few numbers, and the same rules as every
// other creature. It lives in the overworld and comes out in the dark.
int entity_register_type(const char *name, int tile, int health, float speed,
    int damage, int hostile, int drop, int drop_count)
{
    if (type_count >= MAX_MOB_TYPES) {
        return -1;
    }
    int kind = type_count++;
    add_type(kind, name, tile, 0.45, 1.6, health, speed, damage, 24, 1.2,
        hostile ? 1 : 0, 0, 0, drop, drop_count, 0);
    return kind;
}

Entity *entity_at(int index) {
    if (index < 0 || index >= MAX_ENTITIES) {
        return 0;
    }
    return entities + index;
}

Projectile *projectile_at(int index) {
    if (index < 0 || index >= MAX_PROJECTILES) {
        return 0;
    }
    return projectiles + index;
}

int entity_count() {
    int count = 0;
    for (int i = 0; i < MAX_ENTITIES; i++) {
        if (entities[i].used) {
            count++;
        }
    }
    return count;
}

// ---------------------------------------------------------------------------
// the world, as far as a creature needs it
// ---------------------------------------------------------------------------

static int solid(float x, float y, float z) {
    return is_obstacle(entity_block_at(roundf(x), roundf(y), roundf(z)));
}

// Does the column the creature stands in run into something?
static int blocked(MobType *type, float x, float y, float z) {
    float top = y + type->height - 0.1;
    for (float h = y + 0.1; h < top; h += 0.9) {
        if (solid(x, h, z)) {
            return 1;
        }
    }
    return solid(x, top, z);
}

// The first solid block below start_y with two free blocks above it.
static int find_ground(float x, float z, int start_y, float *out_y) {
    for (int y = start_y; y > 4; y--) {
        if (is_obstacle(entity_block_at(roundf(x), y, roundf(z)))) {
            if (entity_block_at(roundf(x), y + 1, roundf(z)) == 0 &&
                entity_block_at(roundf(x), y + 2, roundf(z)) == 0)
            {
                *out_y = y + 0.5;
                return 1;
            }
            return 0;
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// spawning
// ---------------------------------------------------------------------------

int entity_spawn(int kind, float x, float y, float z) {
    MobType *type = mob_type(kind);
    if (!type) {
        return -1;
    }
    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity *e = entities + i;
        if (e->used) {
            continue;
        }
        memset(e, 0, sizeof(Entity));
        e->used = 1;
        e->kind = kind;
        e->x = x;
        e->y = y;
        e->z = z;
        e->health = type->health;
        e->wander_timer = rand_double() * 2;
        return i;
    }
    return -1;
}

void entity_spawn_in_front(int kind, float x, float y, float z, float rx) {
    // Craft looks along (sin rx, -cos rx) -- see get_sight_vector()
    float distance = 4;
    float nx = x + sinf(rx) * distance;
    float nz = z - cosf(rx) * distance;
    float ny = y;
    if (!find_ground(nx, nz, (int)roundf(y) + 3, &ny)) {
        ny = y;
    }
    entity_spawn(kind, nx, ny, nz);
}

void projectile_spawn(
    float x, float y, float z, float vx, float vy, float vz, float damage)
{
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *p = projectiles + i;
        if (p->used) {
            continue;
        }
        p->used = 1;
        p->x = x;
        p->y = y;
        p->z = z;
        p->vx = vx;
        p->vy = vy;
        p->vz = vz;
        p->life = 3;
        p->damage = damage;
        return;
    }
}

// Nature refills the world around the player: hostile creatures in the dark,
// animals in the light, never more than the caps below.
void entities_maybe_spawn(
    double dt, float px, float py, float pz, int dimension, float daylight)
{
    spawn_timer -= dt;
    if (spawn_timer > 0) {
        return;
    }
    spawn_timer = 2.5;

    int hostile = 0;
    int passive = 0;
    for (int i = 0; i < MAX_ENTITIES; i++) {
        if (!entities[i].used) {
            continue;
        }
        MobType *type = mob_type(entities[i].kind);
        if (type->hostile) {
            hostile++;
        }
        else {
            passive++;
        }
    }

    int night = daylight < 0.35 || dimension == 1;
    int want_hostile = night && hostile < 6;
    int want_passive = !night && passive < 5 && dimension == 0;
    if (!want_hostile && !want_passive) {
        return;
    }

    // a ring around the player: close enough to matter, far enough not to
    // appear out of thin air in front of their eyes
    float angle = rand_double() * 6.28318;
    float distance = 18 + rand_double() * 20;
    float x = px + cosf(angle) * distance;
    float z = pz + sinf(angle) * distance;
    float y;
    if (!find_ground(x, z, (int)roundf(py) + 12, &y)) {
        return;
    }

    int candidates[MAX_MOB_TYPES];
    int count = 0;
    for (int kind = 0; kind < type_count; kind++) {
        MobType *type = types + kind;
        if (type->dimension != dimension) {
            continue;
        }
        if (want_hostile && type->hostile) {
            candidates[count++] = kind;
        }
        if (want_passive && !type->hostile) {
            candidates[count++] = kind;
        }
    }
    if (!count) {
        return;
    }
    entity_spawn(candidates[rand_int(count)], x, y, z);
}

// ---------------------------------------------------------------------------
// one tick
// ---------------------------------------------------------------------------

static void move_entity(Entity *e, MobType *type, float vx, float vz, double dt) {
    float nx = e->x + vx * dt;
    float nz = e->z + vz * dt;
    int moved = 0;
    if (!blocked(type, nx, e->y, e->z)) {
        e->x = nx;
        moved = 1;
    }
    if (!blocked(type, e->x, e->y, nz)) {
        e->z = nz;
        moved = 1;
    }
    // a step in the way: hop over it instead of standing there forever
    if (!moved && e->dy == 0 && (vx || vz)) {
        if (!blocked(type, nx, e->y + 1.1, nz)) {
            e->dy = 7;
        }
    }
}

static void fall(Entity *e, MobType *type, double dt) {
    e->dy -= 25 * dt;
    if (e->dy < -60) {
        e->dy = -60;
    }
    float ny = e->y + e->dy * dt;
    if (e->dy < 0 && solid(e->x, ny, e->z)) {
        e->y = roundf(ny) + 0.5;
        e->dy = 0;
    }
    else if (e->dy > 0 && solid(e->x, ny + type->height, e->z)) {
        e->dy = 0;
    }
    else {
        e->y = ny;
    }
    if (e->y < -8) {
        e->used = 0;          // fell out of the world
    }
}

float entities_update(
    double dt, float px, float py, float pz, int dimension,
    float daylight, int paused)
{
    float damage_to_player = 0;
    tick_count++;
    if (dt > 0.25) {
        dt = 0.25;            // after a long pause, do not teleport anything
    }

    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity *e = entities + i;
        if (!e->used) {
            continue;
        }
        MobType *type = mob_type(e->kind);
        float dx = px - e->x;
        float dz = pz - e->z;
        float distance = sqrtf(dx * dx + dz * dz);

        // creatures of the other world, or of the wrong time of day, leave
        if (distance > 70 || type->dimension != dimension) {
            e->used = 0;
            continue;
        }
        if (type->night_only && daylight > 0.6 && dimension == 0) {
            e->used = 0;
            continue;
        }
        if (e->hurt_flash > 0) {
            e->hurt_flash -= dt;
        }
        if (paused) {
            continue;
        }

        float vx = 0;
        float vz = 0;
        int chasing = type->hostile && distance < type->sight && distance > 0.1;
        if (chasing) {
            e->rx = atan2f(dz, dx);
            vx = cosf(e->rx) * type->speed;
            vz = sinf(e->rx) * type->speed;
            if (distance < 1.3 && fabsf(py - e->y) < 2.5) {
                vx = vz = 0;
                e->attack_timer -= dt;
                if (e->attack_timer <= 0) {
                    e->attack_timer = type->attack_rate;
                    damage_to_player += type->damage;
                }
            }
        }
        else {
            // wandering: a new direction every few seconds
            e->wander_timer -= dt;
            if (e->wander_timer <= 0) {
                e->wander_timer = 2 + rand_double() * 4;
                if (rand_double() < 0.35) {
                    e->wander_x = e->wander_z = 0;
                }
                else {
                    float angle = rand_double() * 6.28318;
                    e->wander_x = cosf(angle);
                    e->wander_z = sinf(angle);
                    e->rx = angle;
                }
            }
            vx = e->wander_x * type->speed * 0.4;
            vz = e->wander_z * type->speed * 0.4;
        }

        if (type->hops) {
            // a slime only moves while it is in the air
            if (e->dy == 0) {
                if (vx || vz) {
                    e->dy = 6;
                }
                vx = vz = 0;
            }
        }
        move_entity(e, type, vx, vz, dt);
        fall(e, type, dt);
    }

    // Bolts. A bolt flies 26 blocks a second; on a slow phone a frame is a
    // tenth of a second, so in one step it would cross three blocks -- and
    // fly straight through whatever stood in them. So the flight is walked
    // in quarter-block steps, and each step looks for a block and a body.
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *p = projectiles + i;
        p->hit = 0;
        if (!p->used) {
            continue;
        }
        p->life -= dt;
        if (p->life <= 0) {
            p->used = 0;
            continue;
        }
        p->vy -= 9 * dt;
        float dx = p->vx * dt;
        float dy = p->vy * dt;
        float dz = p->vz * dt;
        float length = sqrtf(dx * dx + dy * dy + dz * dz);
        int steps = (int)(length / 0.25) + 1;
        for (int step = 0; step < steps && p->used; step++) {
            p->x += dx / steps;
            p->y += dy / steps;
            p->z += dz / steps;
            if (solid(p->x, p->y, p->z)) {
                p->used = 0;
                break;
            }
            for (int j = 0; j < MAX_ENTITIES; j++) {
                Entity *e = entities + j;
                if (!e->used) {
                    continue;
                }
                MobType *type = mob_type(e->kind);
                float ex = p->x - e->x;
                float ez = p->z - e->z;
                float ey = p->y - (e->y + type->height / 2);
                if (fabsf(ex) < type->width + 0.35 &&
                    fabsf(ez) < type->width + 0.35 &&
                    fabsf(ey) < type->height / 2 + 0.35)
                {
                    p->hit = j + 1;
                    break;
                }
            }
            if (p->hit) {
                break;
            }
        }
    }
    return damage_to_player;
}

// The creature the player is looking at, walking the line of sight forward in
// small steps -- the same idea as the block hit test, with boxes.
int entity_pick(
    float x, float y, float z, float vx, float vy, float vz, float range)
{
    float step = 0.2;
    for (float t = 0.3; t <= range; t += step) {
        float px = x + vx * t;
        float py = y + vy * t;
        float pz = z + vz * t;
        for (int i = 0; i < MAX_ENTITIES; i++) {
            Entity *e = entities + i;
            if (!e->used) {
                continue;
            }
            MobType *type = mob_type(e->kind);
            // a little more than the creature really is: on a touch screen
            // the crosshair is never exactly where the thumb thinks it is
            float w = type->width + 0.3;
            if (fabsf(px - e->x) < w && fabsf(pz - e->z) < w &&
                py > e->y - 0.5 && py < e->y + type->height + 0.5)
            {
                return i;
            }
        }
    }
    return -1;
}

// What the swing catches when the crosshair missed: the closest creature
// within reach that is roughly in front. A touch screen never aims exactly,
// and a sword that only hits pixel-perfect is no fun on a phone.
int entity_pick_near(
    float x, float y, float z, float vx, float vz, float radius)
{
    float length = sqrtf(vx * vx + vz * vz);
    if (length < 0.001) {
        return -1;
    }
    vx /= length;
    vz /= length;
    int best = -1;
    float best_distance = radius;
    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity *e = entities + i;
        if (!e->used) {
            continue;
        }
        MobType *type = mob_type(e->kind);
        float dx = e->x - x;
        float dz = e->z - z;
        float dy = e->y + type->height / 2 - y;
        float distance = sqrtf(dx * dx + dz * dz);
        if (distance > best_distance || distance < 0.001 || fabsf(dy) > 2.5) {
            continue;
        }
        if ((dx / distance) * vx + (dz / distance) * vz < 0.55) {
            continue;   // behind or to the side
        }
        best = i;
        best_distance = distance;
    }
    return best;
}

int entity_damage(int index, float amount, int *drop_count) {
    Entity *e = entity_at(index);
    if (!e || !e->used) {
        return 0;
    }
    MobType *type = mob_type(e->kind);
    e->health -= amount;
    e->hurt_flash = 0.25;
    // being hit makes an animal run, and a monster commit
    e->wander_timer = 3;
    if (e->health > 0) {
        return 0;
    }
    e->used = 0;
    if (drop_count) {
        *drop_count = type->drop_count;
    }
    return type->drop_item;
}
