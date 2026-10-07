#ifndef PAINTING_VARIANT_H
#define PAINTING_VARIANT_H

#include "resource/resource_id.h"
typedef struct {
    ResourceID asset_id;
    i32 width;
    i32 height;
    string title;
    string author;
} PaintingVariant;    

#endif /* ! PAINTING_VARIANT_H */
