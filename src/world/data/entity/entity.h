#ifndef ENTITY_H
#define ENTITY_H

#include "utils/position.h"
enum EntityComponentType {
    EC_POSITION,
    EC_PLAYER,
    _EC_COUNT,
};

typedef struct {
    long components[_EC_COUNT];
} Entity;

typedef struct {
    Vec3d pos;
} TransformComponent;

void ecomponents_init(void);
void ecomponents_cleanup(void);

void* ecomponents_get(enum EntityComponentType type, long entity_id);
void ecomponents_attach(long entity_id, enum EntityComponentType type, const void* data);
void ecomponents_tick(double delta_time);

long ecomponents_create_entity(void);

#endif /* ! ENTITY_H */
