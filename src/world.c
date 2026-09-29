#include "config.h"
#include "item.h"
#include "noise.h"
#include "world.h"

static int dimension = 0;

void world_set_dimension(int value) {
    dimension = value ? 1 : 0;
}

int world_dimension() {
    return dimension;
}

// The underworld: rock with caves eaten out of it, a sea of lava at the
// bottom, a rock ceiling on top and glowing stone where the caves break the
// surface. Deliberately claustrophobic -- it is the other half of a world
// whose first half is open sky.
static void create_underworld(int p, int q, world_func func, void *arg) {
    int pad = 1;
    for (int dx = -pad; dx < CHUNK_SIZE + pad; dx++) {
        for (int dz = -pad; dz < CHUNK_SIZE + pad; dz++) {
            int flag = 1;
            if (dx < 0 || dz < 0 || dx >= CHUNK_SIZE || dz >= CHUNK_SIZE) {
                flag = -1;
            }
            int x = p * CHUNK_SIZE + dx;
            int z = q * CHUNK_SIZE + dz;
            int last_solid = 0;
            for (int y = 0; y < UNDERWORLD_CEILING + 4; y++) {
                int solid;
                if (y < 3 || y >= UNDERWORLD_CEILING) {
                    solid = 1;              // floor and ceiling close it off
                }
                else {
                    float cave = simplex3(
                        x * 0.021, y * 0.045, z * 0.021, 4, 0.5, 2);
                    // Craft's noise runs from 0 to 1 with its mass around
                    // 0.5 -- a threshold below that would leave no caves at
                    // all. Open down at the lava, dense under the ceiling.
                    float bias = 0.60 - 0.20 * (float)y / UNDERWORLD_CEILING;
                    solid = cave > bias;
                }
                int w = EMBER_ROCK;
                if (solid) {
                    // ash sand in patches, glowing stone at the cave roofs
                    if (simplex2(x * 0.04, z * 0.04, 3, 0.6, 2) > 0.66) {
                        w = ASH_SAND;
                    }
                    else if (!last_solid && y > 6 &&
                        simplex3(x * 0.07, y * 0.07, z * 0.07, 2, 0.5, 2) > 0.66)
                    {
                        w = GLOW_ROCK;
                    }
                    func(x, y, z, w * flag, arg);
                }
                else if (y <= LAVA_LEVEL) {
                    func(x, y, z, LAVA * flag, arg);
                }
                last_solid = solid;
            }
        }
    }
}

void create_world(int p, int q, world_func func, void *arg) {
    if (dimension == 1) {
        create_underworld(p, q, func, arg);
        return;
    }
    int pad = 1;
    for (int dx = -pad; dx < CHUNK_SIZE + pad; dx++) {
        for (int dz = -pad; dz < CHUNK_SIZE + pad; dz++) {
            int flag = 1;
            if (dx < 0 || dz < 0 || dx >= CHUNK_SIZE || dz >= CHUNK_SIZE) {
                flag = -1;
            }
            int x = p * CHUNK_SIZE + dx;
            int z = q * CHUNK_SIZE + dz;
            float f = simplex2(x * 0.01, z * 0.01, 4, 0.5, 2);
            float g = simplex2(-x * 0.01, -z * 0.01, 2, 0.9, 2);
            int mh = g * 32 + 16;
            int h = f * mh;
            int w = 1;
            int t = 12;
            if (h <= t) {
                h = t;
                w = 2;
            }
            // sand and grass terrain
            for (int y = 0; y < h; y++) {
                func(x, y, z, w * flag, arg);
            }
            // water fills everything the land does not reach up to
            for (int y = h; y < SEA_LEVEL; y++) {
                func(x, y, z, WATER * flag, arg);
            }
            if (w == 1 && h >= SEA_LEVEL) {
                if (SHOW_PLANTS) {
                    // grass
                    if (simplex2(-x * 0.1, z * 0.1, 4, 0.8, 2) > 0.6) {
                        func(x, h, z, 17 * flag, arg);
                    }
                    // flowers
                    if (simplex2(x * 0.05, -z * 0.05, 4, 0.8, 2) > 0.7) {
                        int w = 18 + simplex2(x * 0.1, z * 0.1, 4, 0.8, 2) * 7;
                        func(x, h, z, w * flag, arg);
                    }
                }
                // trees
                int ok = SHOW_TREES;
                if (dx - 4 < 0 || dz - 4 < 0 ||
                    dx + 4 >= CHUNK_SIZE || dz + 4 >= CHUNK_SIZE)
                {
                    ok = 0;
                }
                if (ok && simplex2(x, z, 6, 0.5, 2) > 0.84) {
                    for (int y = h + 3; y < h + 8; y++) {
                        for (int ox = -3; ox <= 3; ox++) {
                            for (int oz = -3; oz <= 3; oz++) {
                                int d = (ox * ox) + (oz * oz) +
                                    (y - (h + 4)) * (y - (h + 4));
                                if (d < 11) {
                                    func(x + ox, y, z + oz, 15, arg);
                                }
                            }
                        }
                    }
                    for (int y = h; y < h + 7; y++) {
                        func(x, y, z, 5, arg);
                    }
                }
            }
            // clouds
            if (SHOW_CLOUDS) {
                for (int y = 64; y < 72; y++) {
                    if (simplex3(
                        x * 0.01, y * 0.1, z * 0.01, 8, 0.5, 2) > 0.75)
                    {
                        func(x, y, z, 16 * flag, arg);
                    }
                }
            }
        }
    }
}
