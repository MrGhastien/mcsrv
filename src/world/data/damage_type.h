#ifndef DAMAGE_TYPE_H
#define DAMAGE_TYPE_H

#include "definitions.h"
#include "utils/string.h"

typedef struct {
    string message_id;
    f32 exhaustion;
    enum {
        DMG_TYPE_SCALE_NEVER,
        DMG_TYPE_SCALE_ALWAYS,
        DMG_TYPE_SCALE_WHEN_CAUSED_BY_LIVING_NON_PLAYER,
    } scaling;
    enum {
        DMG_TYPE_EFFECT_HURT,
        DMG_TYPE_EFFECT_THORNS,
        DMG_TYPE_EFFECT_DROWNING,
        DMG_TYPE_EFFECT_BURNING,
        DMG_TYPE_EFFECT_POKING,
        DMG_TYPE_EFFECT_FREEZING,
    } effects;
    enum {
        DMG_TYPE_MSG_TYPE_DEFAULT,
        DMG_TYPE_MSG_TYPE_FALL_VARIANTS,
        DMG_TYPE_MSG_TYPE_INTENTIONAL_GAME_DESIGN,
    } death_message_type;
} DamageType;

#endif /* ! DAMAGE_TYPE_H */
