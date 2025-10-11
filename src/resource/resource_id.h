#ifndef RESOURCE_ID_H
#define RESOURCE_ID_H

#include "utils/string.h"

#define RESID_UNWRAP(resid) cstr(&(resid).namespace), cstr(&(resid).path)
#define RESID_FORMAT "[%s:%s]"

#define STATIC_RESID(namespace_arg, path_arg)                                                      \
    {                                                                                              \
        .namespace = STR_STATIC(namespace_arg),                                                    \
        .path      = STR_STATIC(path_arg),                                                         \
    }

typedef struct resid {
    string namespace;
    string path;
} ResourceID;

extern const Comparator CMP_RESID;

ResourceID resid_create(const string* namespace, const string* path, Arena* arena);
bool resid_parse(const string* id, Arena* arena, ResourceID* out_parsed);
ResourceID resid_default(const string* path, Arena* arena);
ResourceID resid_default_cstr(const char* path);

bool resid_is_namespace(const ResourceID* id, const char* name);
bool resid_is_path(const ResourceID* id, const char* name);
bool resid_is_cstr(const ResourceID* id, const char* name);
bool resid_is(const ResourceID* id, const ResourceID* other);

#endif /* ! RESOURCE_ID_H */
