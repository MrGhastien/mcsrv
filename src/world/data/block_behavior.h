#ifndef BLOCK_BEHAVIOR_H
#define BLOCK_BEHAVIOR_H

#include "definitions.h"
#include "utils/direction.h"
#include "utils/position.h"
#include "level.h"
#include "explosion.h"
#include "world/physics/block_hit.h"
#include "world/data/entity/player.h"
#include "world/physics/shape.h"

typedef struct block_state BlockState;
typedef struct block Block;
typedef struct level Level;

enum PathFindingType {
    LAND,
    WATER,
    AIR,
};

enum UseResult {
    USE_RESULT_SUCCESS,
    USE_RESULT_SUCCESS_NOT_USED,
    USE_RESULT_CONSUME,
    USE_RESULT_CONSUME_PARTIAL,
    USE_RESULT_PASS,
    USE_RESULT_FAIL,
};

enum ItemUseResult {
    IUSE_RESULT_SUCCESS,
    IUSE_RESULT_CONSUME,
    IUSE_RESULT_CONSUME_PARTIAL,
    IUSE_RESULT_DEFAULT_BLOCK_INTERACTION,
    IUSE_RESULT_SKIP_BLOCK_INTERACTION,
    IUSE_RESULT_FAIL,
};

enum BlockRenderType {
    BLOCK_RENDER_TYPE_INVISIBLE,
    BLOCK_RENDER_TYPE_ANIMATED,
    BLOCK_RENDER_TYPE_MODEL,
};

enum Rotation {
    ROTATION_NONE,
    ROTATION_CW_90,
    ROTATION_CW_180,
    ROTATION_CCW_90,
};

enum Mirror {
    MIRROR_NONE,
    MIRROR_Z,
    MIRROR_X,
};

typedef struct placement_context {
    // TODO;
} PlacementContext;

typedef struct block_behavior {
    // TODO: add block behavior function pointers
    void (*prepare_neighbor_shape)(const BlockState* state, Level* level, Vec3i, i32 flags, i32 max_update_depth);
    bool (*pathfindable)(const BlockState* state, enum PathFindingType type);
    const BlockState* (*update_shape)(const BlockState* state, enum Direction direction, const BlockState* neighborState, Level* level, Vec3i pos, Vec3i neighborPos);
    bool (*should_skip_rendering)(const BlockState* state, const BlockState* previous_state, enum Direction side);
    void (*on_neighbor_update)(const BlockState* state, Level* level, Vec3i pos, const Block* neighbor_block, Vec3i neighbor_pos, bool notify);
    void (*new_state_change)(const BlockState* new_state, Level* level, Vec3i pos, const BlockState* old_state, bool notify);
    void (*old_state_change)(const BlockState* old_state, Level* level, Vec3i pos, const BlockState* new_state, bool moved);
    void (*on_exploded)(const BlockState* state, Level* level, Vec3i pos, const Explosion* explosion, void* stack_merger); // TODO stack merger type
    enum UseResult (*on_use_no_item)(const BlockState* state, Level* level, Vec3i pos, Player* player, BlockHitResult* hit);
    enum ItemUseResult (*on_use_with_item)(const BlockState* state, Level* level, Vec3i pos, Player* player, BlockHitResult* hit, void* item); // TODO: Item
    bool (*on_block_event)(const BlockState* state, Level* level, Vec3i pos, i32 type, i32 data);

    // Getter functions
    enum BlockRenderType (*get_render_type)(const BlockState* state); // Always returns a single constant in all cases
    bool (*is_side_transparent)(const BlockState* state);
    bool (*emits_redstone)(const BlockState* state);
    //const FluidState* (*get_fluid_state)(const BlockState* state); // TODO FluidState
    bool (*has_comparator_output)(const BlockState* state);
    //const FeatureFlagSet* (*get_required_features)(void); // simple getter, add member variable
    const BlockState* (*rotate)(const BlockState* state, enum Rotation rotation);
    const BlockState* (*mirror)(const BlockState* state, enum Mirror mirror);
    bool (*can_item_replace)(const BlockState* state, PlacementContext* ctx);
    //bool (*can_fluid_replace)(const BlockState* state, const Fluid* fluid);
    //void (*get_drops)(const BlockState* state, LootParams* params, Vector* out_drops);
    i64 (*get_seed)(const BlockState* state, Vec3i pos);
    const VoxelShape* (*get_occlusion_shape)(const BlockState* state, Level* level, Vec3i pos);
    const VoxelShape* (*get_block_support_shape)(const BlockState* state, Level* level, Vec3i pos);
    const VoxelShape* (*get_interact_shape)(const BlockState* state, Level* level, Vec3i pos);
    i32 (*get_opacity)(const BlockState* state, Level* level, Vec3i pos);
} BlockBehavior;

#endif /* ! BLOCK_BEHAVIOR_H */
