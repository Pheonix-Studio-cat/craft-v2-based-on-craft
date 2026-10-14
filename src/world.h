#ifndef _world_h_
#define _world_h_

typedef void (*world_func)(int, int, int, int, void *);

// 0 is the overworld, 1 the underworld. Each dimension has its own world
// file, so the same chunk coordinates mean different places.
void world_set_dimension(int dimension);
int world_dimension();

void create_world(int p, int q, world_func func, void *arg);

#endif
