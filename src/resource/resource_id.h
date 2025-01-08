#ifndef RESOURCE_ID_H
#define RESOURCE_ID_H

#include "utils/string.h"

typedef struct resid {
    string namespace;
    string path;
} ResourceID;

ResourceID resid_create(const string* namespace, const string* path, Arena* arena);
bool resid_parse(const string* id, Arena* arena, ResourceID* out_parsed);
ResourceID resid_default(const string* path, Arena* arena);
ResourceID resid_default_cstr(const char* path);

bool resid_is_namespace(const ResourceID* id, const char* name);
bool resid_is_path(const ResourceID* id, const char* name);
bool resid_is(const ResourceID* id, const char* name);

#endif /* ! RESOURCE_ID_H */
