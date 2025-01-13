#ifndef EXPLOSION_H
#define EXPLOSION_H

#include "definitions.h"
#include "world/level.h"
#include "utils/position.h"

typedef struct explosion {
    Level* level;
    bool inciendary;
    Vec3d pos;
    f32 radius;
} Explosion;

// TODO: explosion logic

#endif /* ! EXPLOSION_H */
