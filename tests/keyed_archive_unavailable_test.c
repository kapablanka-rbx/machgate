#include "../providers/foundation/archive/objects.c"

struct foundation_class OBJC_CLASS_$_NSProcessInfo;
struct foundation_class OBJC_CLASS_$_NSString;

void* foundation_retain(void* object)
{
    return object;
}

const char* foundation_archive_guest_class_name(const void* class_object)
{
    return class_object;
}

int main(void)
{
    const char* selectors[] = {
        "archivedDataWithRootObject:requiringSecureCoding:error:",
        "unarchivedObjectOfClass:fromData:error:",
    };
    const char* classes[] = {"NSKeyedArchiver", "NSKeyedUnarchiver"};
    uintptr_t object = 42;
    for (size_t index = 0; index < 2; ++index) {
        void* error = NULL;
        uintptr_t result = 1;
        int handled = dispatch_keyed_archiver((void*)classes[index],
            selectors[index], (uintptr_t)&object, (uintptr_t)&error, &result);
        if (!handled || result || !error) {
            fprintf(stderr, "unsupported keyed archive operation returned data or omitted its error\n");
            return 1;
        }
        struct shim_objc_error* archive_error = error;
        if (strcmp(archive_error->domain, "NSCocoaErrorDomain") != 0 ||
            archive_error->code != 3328)
            return 2;
        free(error);
        result = 1;
        if (!dispatch_keyed_archiver((void*)classes[index], selectors[index],
                (uintptr_t)&object, 0, &result) || result)
            return 3;
    }
    uintptr_t result = 17;
    if (dispatch_keyed_archiver("NSKeyedArchiver", "unrelatedSelector",
            0, 0, &result) || result != 17)
        return 4;
    return 0;
}
