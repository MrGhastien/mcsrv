#include "resource_id.h"
#include "utils/hash.h"
#include "utils/string.h"
#include <string.h>

i32 resid_compare_raw(const void* lhs, const void* rhs) {
    const ResourceID* lhs_id = lhs;
    const ResourceID* rhs_id = rhs;

    i32 namespace_comparison = str_compare(&lhs_id->namespace, &rhs_id->namespace);
    if (namespace_comparison != 0)
        return namespace_comparison;
    return str_compare(&lhs_id->path, &rhs_id->path);
}

u64 resid_hash(const void* ptr) {
    const ResourceID* id = ptr;

    u64 h = default_hash(id->namespace.base, id->namespace.length);
    h     = default_hash_acc(h, ":", 1);
    h     = default_hash_acc(h, id->path.base, id->path.length);
    return h;
}

const Comparator CMP_RESID = {
    .hfunc = &resid_hash,
    .comp  = &resid_compare_raw,
};

ResourceID resid_create(const string* namespace, const string* path, Arena* arena) {
    return (ResourceID) {
        .namespace = str_create_copy(namespace, arena),
        .path      = str_create_copy(path, arena),
    };
}

bool resid_parse(const string* id, Arena* arena, ResourceID* out_parsed) {
    i64 idx = str_find_char(id, ':');
    if (idx == -1)
        return FALSE;

    *out_parsed = (ResourceID) {
        .namespace = str_copy_substring(id, 0, idx, arena),
        .path      = str_copy_substring(id, idx + 1, -1, arena),
    };
    return TRUE;
}

ResourceID resid_default(const string* path, Arena* arena) {
    return (ResourceID) {
        .namespace = str_view("minecraft"),
        .path      = str_create_copy(path, arena),
    };
}

ResourceID resid_default_cstr(const char* path) {
    return (ResourceID) {
        .namespace = str_view("minecraft"),
        .path      = str_view(path),
    };
}

bool resid_is_namespace(const ResourceID* id, const char* name) {
    string view = str_view(name);
    return str_compare(&id->namespace, &view) == 0;
}
bool resid_is_path_cstr(const ResourceID* id, const char* name) {
    return resid_is_path(id, str_view(name));
}

bool resid_is_path(const ResourceID* id, const string name) {
    return str_compare(&id->path, &name) == 0;
}
bool resid_is_cstr(const ResourceID* id, const char* name) {
    string name_view = str_view(name);
    i64 idx          = str_find_char(&name_view, ':');
    if (idx == -1)
        return FALSE;

    string namespace = str_substring(&name_view, 0, idx);
    string path      = str_substring(&name_view, idx + 1, -1);

    return str_compare(&id->namespace, &namespace) == 0 && str_compare(&id->path, &path) == 0;
}

bool resid_is(const ResourceID* id, const ResourceID* other) {
    return str_compare(&id->namespace, &other->namespace) == 0 && str_compare(&id->path, &other->path) == 0;
}

string resid_to_string(const ResourceID* id, Arena* arena) {
    u64 ns_length = id->namespace.length;
    u64 path_length = id->path.length;
    u64 total_length = ns_length + path_length + 1;

    string str = str_alloc(total_length, arena);
    memcpy(str.base, id->namespace.base, ns_length);
    memcpy(str.base + ns_length + 1, id->path.base, path_length);
    str.base[ns_length] = ':';

    return str;
}
