#include "../providers/foundation/archive/objects.c"

struct foundation_class OBJC_CLASS_$_NSProcessInfo;
struct foundation_class OBJC_CLASS_$_NSString;

void* foundation_retain(void* object)
{
    return object;
}

void foundation_release(void* object)
{
    (void)object;
}

struct foundation_string* foundation_string_resolve(const void* object)
{
    (void)object;
    return NULL;
}

const char* foundation_archive_guest_class_name(const void* class_object)
{
    return class_object;
}

static int string_equals(void* object, const char* expected)
{
    return shim_objc_is_object(object, SHIM_OBJC_STRING) &&
        strcmp(((struct shim_objc_string*)object)->utf8, expected) == 0;
}

int main(void)
{
    struct shim_objc_dictionary* root = shim_objc_make_dictionary(NULL, NULL, 0);
    if (!root)
        return 1;
    void* key = shim_objc_make_string("TestKey");
    void* value = shim_objc_make_string("UpdatedValue");
    void* utf8_key = shim_objc_make_string("\xe9\x91\xb0\xe5\x8c\x99");
    void* utf8_value = shim_objc_make_string("\xe6\xb8\xac\xe8\xa9\xa6");
    if (!key || !value || !utf8_key || !utf8_value)
        return 2;
    shim_objc_dictionary_set(root, key, value);
    shim_objc_dictionary_set(root, utf8_key, utf8_value);

    uintptr_t arguments[6] = {0};
    arguments[0] = (uintptr_t)root;
    uintptr_t archive_result = 0;
    if (!dispatch_keyed_archiver(&OBJC_CLASS_$_NSKeyedArchiver,
            "archivedDataWithRootObject:requiringSecureCoding:error:", arguments,
            &archive_result) ||
        !archive_result ||
        !shim_objc_is_object((void*)archive_result, SHIM_OBJC_INSTANCE_NSDATA))
        return 3;

    struct shim_objc_data* archive_data = (void*)archive_result;
    uintptr_t unarchive_arguments[6] = {0};
    unarchive_arguments[1] = archive_result;
    uintptr_t decoded = 0;
    if (!dispatch_keyed_archiver(&OBJC_CLASS_$_NSKeyedUnarchiver,
            "unarchivedObjectOfClass:fromData:error:", unarchive_arguments,
            &decoded) ||
        !decoded ||
        !shim_objc_is_object((void*)decoded, SHIM_OBJC_INSTANCE_NSMUTABLEDICTIONARY))
        return 4;

    struct shim_objc_dictionary* decoded_dictionary = (void*)decoded;
    if (decoded_dictionary->pair_count != root->pair_count)
        return 5;
    if (!string_equals(shim_objc_dictionary_get(decoded_dictionary, key),
            "UpdatedValue") ||
        !string_equals(shim_objc_dictionary_get(decoded_dictionary, utf8_key),
            "\xe6\xb8\xac\xe8\xa9\xa6"))
        return 6;
    if (shim_objc_dictionary_get(decoded_dictionary,
            shim_objc_make_string("EmptyKey")))
        return 7;

    uintptr_t invalid_arguments[6] = {0};
    invalid_arguments[1] = (uintptr_t)"not an archive";
    uintptr_t invalid_result = 1;
    if (!dispatch_keyed_archiver(&OBJC_CLASS_$_NSKeyedUnarchiver,
            "unarchivedObjectOfClass:fromData:error:", invalid_arguments,
            &invalid_result) ||
        invalid_result)
        return 8;

    uintptr_t unrelated_result = 17;
    if (dispatch_keyed_archiver(&OBJC_CLASS_$_NSKeyedArchiver,
            "unrelatedSelector", arguments, &unrelated_result) ||
        unrelated_result != 17)
        return 9;
    return 0;
}
