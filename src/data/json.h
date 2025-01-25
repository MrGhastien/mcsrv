#ifndef JSON_H
#define JSON_H

#include "containers/vector.h"
#include "definitions.h"
#include "utils/iomux.h"
#include "utils/string.h"

enum JSONType {
    JSON_STRING,
    JSON_INT,
    JSON_FLOAT,
    JSON_OBJECT,
    JSON_ARRAY,
    JSON_BOOL,
    JSON_NULL,
    _JSON_COUNT
};

union JSONSimpleValue {
    bool boolean;
    i64 number;
    f64 fnumber;
};


typedef struct json {
    Arena* arena;
    Vector tokens;
    Vector stack;
} JSON;

enum JSONStatus {
    JSONE_OK = 0,
    JSONE_INVALID_TYPE,
    JSONE_INCOMPATIBLE_TYPE,
    JSONE_NO_ROOT,
    JSONE_INVALID_PARENT,
    JSONE_MAX_NESTING,
    JSONE_NOT_FOUND,
    JSONE_IO,
    JSONE_UNKNOWN_TOKEN,
    JSONE_UNEXPECTED_TOKEN,
    JSONE_MISSING_TOKEN,
    JSONE_ROOT_ALREADY_PRESENT,
};

/**
 * Creates a new JSON tree.
 *
 * @param[in] arena The arena used to allocate memory for the JSON tree.
 * @param[in] max_token_count Maximum amount of tokens (nodes) in this JSON tree.
 * @return The newly initialized JSON tree.
 */
JSON json_create(Arena* arena, u64 max_token_count);

/**
 * Sets the root token of a JSON tree.
 *
 * @param[in,out] json The JSON tree for which to set the root token.
 * @param[in] type The type of token to set as the root.
 * @return The status of the operation.
 */
enum JSONStatus json_set_root(JSON* json, enum JSONType type);

/**
 * Adds a simple value token to the current JSON list, and selects it.
 *
 * If the current token is not a list, this function does nothing.
 *
 * @param[in] json The JSON tree containing the list to push into.
 * @param[in] type The type of token to push.
 * @param[in] value A union containing the value to put. Ignored if @ref type is not @ref JSON_INT,
 * @ref JSON_FLOAT or @ref JSON_BOOL.
 */
enum JSONStatus json_push_simple(JSON* json, enum JSONType type, union JSONSimpleValue value);
/**
 * Adds a string token to the current JSON list, and selects it.
 *
 * If the current token is not a list this function does nothing.
 *
 * @param[in] json The JSON tree containing the list to push into.
 * @param[in] str The string to push.
 */
enum JSONStatus json_push_str(JSON* json, const string* str);
/**
 * Adds a token to the current JSON list, and selects it.
 *
 * The newly pushed token is initialized to zero. It can be set using the `json_set_*` family of
 * functions. If the current token is not a list this function does nothing.
 *
 * @param[in] json The JSON tree containing the list to push into.
 * @param[in] type The type of token to push.
 */
enum JSONStatus json_push(JSON* json, enum JSONType type);

/**
 * Adds a simple value token to the current JSON object, and selects it.
 *
 * If the current token is not an object this function does nothing.
 *
 * @param[in] json The JSON tree containing the object to put into.
 * @param[in] name The name of the token to put into the current object.
 * @param[in] type The type of the token.
 * @param[in] value A union containing the value to put. Ignored if @ref type is not @ref JSON_INT,
 * @ref JSON_FLOAT or @ref JSON_BOOL.
 */
enum JSONStatus
json_put_simple(JSON* json, const string* name, enum JSONType type, union JSONSimpleValue value);
/**
 * Adds a string token to the current JSON object, and selects it.
 *
 * If the current token is not an object this function does nothing.
 *
 * @param[in] json The JSON tree containing the object to put into.
 * @param[in] name The name of the token to put into the current object.
 * @param[in] str The string value of the token.
 */
enum JSONStatus json_put_str(JSON* json, const string* name, const string* str);
/**
 * Adds a token to the current JSON object, and selects it.
 *
 * If the current token is not an object this function does nothing.
 *
 * @param[in] json The JSON tree containing the object to put into.
 * @param[in] name The name of the token to put into the current object.
 * @param[in] type The type of the token.
 */
enum JSONStatus json_put(JSON* json, const string* name, enum JSONType type);

/**
 * Adds a simple value token to the current JSON object, and selects it.
 *
 * If the current token is not an object this function does nothing.
 *
 * @param[in] json The JSON tree containing the object to put into.
 * @param[in] name The name of the token to put into the current object.
 * @param[in] type The type of the token.
 * @param[in] value A union containing the value to put. Ignored if @ref type is not @ref JSON_INT,
 * @ref JSON_FLOAT or @ref JSON_BOOL.
 */
enum JSONStatus
json_cstr_put_simple(JSON* json, const char* name, enum JSONType type, union JSONSimpleValue value);
/**
 * Adds a string token to the current JSON object, and selects it.
 *
 * If the current token is not an object this function does nothing.
 *
 * @param[in] json The JSON tree containing the object to put into.
 * @param[in] name The name of the token to put into the current object.
 * @param[in] str The string value of the token.
 */
enum JSONStatus json_cstr_put_str(JSON* json, const char* name, const string* str);
/**
 * Adds a token to the current JSON object, and selects it.
 *
 * If the current token is not an object this function does nothing.
 *
 * @param[in] json The JSON tree containing the object to put into.
 * @param[in] name The name of the token to put into the current object.
 * @param[in] type The type of the token.
 */
enum JSONStatus json_cstr_put(JSON* json, const char* name, enum JSONType type);

/**
 * Sets the value of the current boolean token.
 *
 * @param[in] json The JSON tree containing the token to set.
 * @param[in] value The value to set the token to.
 */
enum JSONStatus json_set_bool(JSON* json, bool value);

/**
 * Sets the value of the current integer token.
 *
 * @param[in] json The JSON tree containing the token to set.
 * @param[in] value The value to set the token to.
 */
enum JSONStatus json_set_int(JSON* json, i64 value);

/**
 * Sets the value of the current floating-point number token.
 *
 * @param[in] json The JSON tree containing the token to set.
 * @param[in] value The value to set the token to.
 */
enum JSONStatus json_set_float(JSON* json, f64 value);

/**
 * Writes a JSON tree to a file.
 *
 * @param[in] json The JSON tree to save.
 * @param[in] path The path of the output file.
 */
enum JSONStatus json_write_file(const JSON* json, const string* path);
/**
 * Writes a JSON tree to an output stream.
 *
 * @param[in] json The JSON tree to save.
 * @param[in] multiplexer An IO multiplexer used to write to the output stream.
 * @param[in] network @ref TRUE if the JSON shall be serialized to be sent through the network, @ref
 * FALSE otherwise.
 */
enum JSONStatus json_write(const JSON* json, IOMux multiplexer);

enum JSONStatus json_to_string(const JSON* json, Arena* arena, string* out_str);

/* === Parsing part === */

/**
 * Reads and creates a JSON tree from an input stream.
 *
 * This function initializes and populates a new JSON tree by reading the giben input stream.
 * The file can be compressed using gzip.
 *
 * @param[in] multiplexer An IO multiplexer used to read from the input stream.
 * @param[in] arena The arena used to allocate memory for the tree.
 * @param[out] out_json A pointer to an uninitialized JSON tree.
 * @return @ref TRUE if the parsing completed successfully, @ref FALSE if an error occurred.
 */
enum JSONStatus json_parse(IOMux multiplexer, Arena* arena, JSON* out_json);

/**
 * Moves the current token pointer to the child token with the given name.
 *
 * If the current token is not a `JSON_OBJECT` token, this function does nothing.
 *
 * @param[in] json The JSON tree.
 * @param[in] name The name of the child token to find.
 */
enum JSONStatus json_move_to_name(JSON* json, const string* name);

/**
 * Moves the current token pointer to the child token with the given name.
 *
 * If the current token is not a `JSON_OBJECT` token, this function does nothing.
 * 
 * @note This is the equivalent of @ref json_move_to_name for C strings.
 *
 * @param[in] json The JSON tree.
 * @param[in] name The name of the child token to find.
 */
enum JSONStatus json_move_to_cstr(JSON* json, const char* name);
/**
 * Moves the current token pointer to the child token at the given index.
 *
 * If the current token is not a `JSON_LIST` token, this function does nothing.
 *
 * @param[in] json The JSON tree.
 * @param[in] index The index of the child token to find.
 */
enum JSONStatus json_move_to_index(JSON* json, i32 index);
/**
 * Moves the current token pointer to the parent of the current token.
 *
 * If the current token is the root token, this function does nothing.
 *
 * @param[in] json The JSON tree.
 */
enum JSONStatus json_move_to_parent(JSON* json);
/**
 * Moves the current token pointer to the next sibling of the current token.
 *
 * If the current token has no sibling or is the last child of a list or object token,
 * this function does nothing.
 *
 * @param[in] json The JSON tree.
 */
enum JSONStatus json_move_to_next_sibling(JSON* json);
/**
 * Moves the current token pointer to the previous sibling of the current token.
 *
 * If the current token has no sibling or is the last child of a list or object token,
 * this function does nothing.
 *
 * @param[in] json The JSON tree.
 */
enum JSONStatus json_move_to_prev_sibling(JSON* json);

/**
 * Retrieves the boolean value of the current token.
 *
 * If the current token is not a `JSON_BOOL` token, this function aborts the server.
 *
 * @param[in] json The JSON tree.
 * @return The byte value.
 */
i8 json_get_bool(JSON* json);
/**
 * Retrieves the integer value of the current token.
 *
 * If the current token is not a `JSON_INT` token, this function aborts the server.
 *
 * @param[in] json The JSON tree.
 * @return The integer value.
 */
i64 json_get_int(JSON* json);
/**
 * Retrieves the floating-point value of the current token.
 *
 * If the current token is not a `JSON_FLOAT` token, this function aborts the server.
 *
 * @param[in] json The JSON tree.
 * @return The floating-point value.
 */
f64 json_get_float(JSON* json);

/**
 * Retrieves the length of the current object or list token.
 *
 * If the current token is not a `JSON_OBJECT` of `JSON_ARRAY` token, this function aborts the server.
 *
 * @param[in] json The JSON tree.
 * @return The length of the array.
 */
i64 json_get_length(JSON* json);
/**
 * Retrieves the name of the current token.
 *
 * If the parent of the current token is a list or array token, this functions return `NULL`.
 *
 * @param[in] json The JSON tree.
 * @return A pointer to the name of the token, or NULL if the token cannot have a name. The returned
 * string can be modified.
 */
string* json_get_name(JSON* json);
/**
 * Retrieves the string value of the current token.
 *
 * If the current token is not a `JSON_STRING` token, this function aborts the server.
 *
 * @param[in] json The JSON tree.
 * @return A pointer to the underlying string value. The string can be modified.
 */
string* json_get_string(JSON* json);

#endif /* ! JSON_H */
