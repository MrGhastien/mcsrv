#ifndef BLOCK_BEHAVIOR_H
#define BLOCK_BEHAVIOR_H

#include "definitions.h"
#include "utils/direction.h"
#include "utils/position.h"
#include "world/level.h"

typedef struct block_state BlockState;
typedef struct block Block;

enum PathFindingType {
    LAND,
    WATER,
    AIR,
};

typedef struct block_behavior {
    // TODO: add block behavior function pointers
    void (*prepareNeighborShape)(const BlockState* state, Level* level, Vec3i, i32 flags, i32 max_update_depth);
    bool (*pathfindable)(const BlockState* state, enum PathFindingType type);
    const BlockState* (*update_shape)(const BlockState* state, enum Direction direction, const BlockState* neighborState, Level* level, Vec3i pos, Vec3i neighborPos);
    bool (*should_skip_rendering)(const BlockState* state, const BlockState* previous_state, enum Direction side);
    void (*on_neighbor_update)(const BlockState* state, Level* level, Vec3i pos, const Block* neighbor_block, Vec3i neighbor_pos, bool notify);
    void (*new_state_change)(const BlockState* new_state, Level* level, Vec3i pos, const BlockState* old_state, bool notify);
    void (*old_state_change)(const BlockState* old_state, Level* level, Vec3i pos, const BlockState* new_state, bool moved);
} BlockBehavior;

#endif /* ! BLOCK_BEHAVIOR_H */
