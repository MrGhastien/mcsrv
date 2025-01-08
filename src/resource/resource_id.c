#include "resource_id.h"
#include "utils/string.h"

ResourceID resid_create(const string* namespace, const string* path, Arena* arena) {
    return (ResourceID){
        .namespace = str_create_copy(namespace, arena),
        .path = str_create_copy(path, arena),
    };
}

bool resid_parse(const string* id, Arena* arena, ResourceID* out_parsed) {
    i64 idx = str_find_char(id, ':');
    if (idx == -1)
        return FALSE;

    *out_parsed = (ResourceID){
        .namespace = str_copy_substring(id, 0, idx, arena),
        .path = str_copy_substring(id, idx + 1, -1, arena),
    };
    return TRUE;
}

ResourceID resid_default(const string* path, Arena* arena) {
    return (ResourceID){
        .namespace = str_create_view("minecraft"),
        .path = str_create_copy(path, arena),
    };
}

ResourceID resid_default_cstr(const char* path) {
    return (ResourceID){
        .namespace = str_create_view("minecraft"),
        .path = str_create_view(path),
    };
}

bool resid_is_namespace(const ResourceID* id, const char* name) {
    string view = str_create_view(name);
    return str_compare(&id->namespace, &view) == 0;
}
bool resid_is_path(const ResourceID* id, const char* name) {
    string view = str_create_view(name);
    return str_compare(&id->path, &view) == 0;
}
bool resid_is(const ResourceID* id, const char* name) {
    string name_view = str_create_view(name);
    i64 idx = str_find_char(&name_view, ':');
    if (idx == -1)
        return FALSE;

    string namespace = str_substring(&name_view, 0, idx);
    string path = str_substring(&name_view, idx + 1, -1);

    return str_compare(&id->namespace, &namespace) == 0 && str_compare(&id->path, &path) == 0;
}
