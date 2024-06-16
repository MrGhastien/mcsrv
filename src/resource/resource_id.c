#include "resource_id.h"
#include "utils/string.h"

ResourceID resid_create(const string* namespace, const string* path, Arena* arena) {
    return (ResourceID){
        .namespace = str_create_from(namespace, arena),
        .path      = str_create_from(path, arena),
    };
}

ResourceID resid_default(const string* path, Arena* arena) {
    return (ResourceID){
        .namespace = str_create_const("minecraft"),
        .path      = str_create_from(path, arena),
    };
}

ResourceID resid_default_cstr(const char* path) {
    return (ResourceID){
        .namespace = str_create_const("minecraft"),
        .path      = str_create_const(path),
    };
}

