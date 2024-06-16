#ifndef RESOURCE_ID_H
#define RESOURCE_ID_H

#include "utils/string.h"

typedef struct resid {
    string namespace;
    string path;
} ResourceID;

ResourceID resid_create(const string* namespace, const string* path, Arena* arena);
ResourceID resid_default(const string* path, Arena* arena);
ResourceID resid_default_cstr(const char* path);

#endif /* ! RESOURCE_ID_H */
