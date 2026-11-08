#include "runtime.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_GUEST_CLASSES 4096
#define MAX_GUEST_METHODS 4096

struct guest_class_layout {
    void* isa;
    void* superclass;
    void* cache[2];
    uintptr_t data;
};

struct guest_method {
    const char* selector;
    void* implementation;
};

struct guest_class {
    struct guest_class_layout* address;
    const char* name;
    size_t instance_size;
    struct guest_method* instance_methods;
    size_t instance_method_count;
    struct guest_method* class_methods;
    size_t class_method_count;
};

struct readable_range { uintptr_t start, end; };
static struct readable_range* ranges;
static size_t range_count;
static pthread_mutex_t ranges_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct guest_class classes[MAX_GUEST_CLASSES];
static size_t class_count;
static pthread_mutex_t classes_mutex = PTHREAD_MUTEX_INITIALIZER;

static void refresh_ranges(void)
{
    FILE* maps = fopen("/proc/self/maps", "r");
    if (!maps)
        return;
    range_count = 0;
    char line[512];
    while (fgets(line, sizeof(line), maps)) {
        uintptr_t start, end;
        char permissions[5];
        if (sscanf(line, "%lx-%lx %4s", &start, &end, permissions) != 3 ||
            permissions[0] != 'r')
            continue;
        struct readable_range* result = realloc(ranges,
            (range_count + 1) * sizeof(*ranges));
        if (!result)
            break;
        ranges = result;
        ranges[range_count++] = (struct readable_range){start, end};
    }
    fclose(maps);
}

int foundation_archive_readable(const void* address, size_t size)
{
    uintptr_t start = (uintptr_t)address;
    if (start < 4096 || size > UINTPTR_MAX - start)
        return 0;
    pthread_mutex_lock(&ranges_mutex);
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (!range_count || attempt)
            refresh_ranges();
        for (size_t index = 0; index < range_count; ++index) {
            if (start >= ranges[index].start && start + size <= ranges[index].end) {
                pthread_mutex_unlock(&ranges_mutex);
                return 1;
            }
        }
    }
    pthread_mutex_unlock(&ranges_mutex);
    return 0;
}

static struct guest_class* find_class(void* address)
{
    for (size_t index = 0; index < class_count; ++index)
        if (classes[index].address == address)
            return &classes[index];
    return NULL;
}

static const unsigned char* class_data(void* address)
{
    if (!foundation_archive_readable(address, sizeof(struct guest_class_layout)))
        return NULL;
    const struct guest_class_layout* layout = address;
    if (*(const uint32_t*)address == 0x4f424a43u) return NULL;
    const unsigned char* data = (void*)(layout->data & ~(uintptr_t)7);
    return foundation_archive_readable(data, 40) ? data : NULL;
}

static struct guest_method* parse_methods(void* class_object, size_t* count)
{
    const unsigned char* data = class_data(class_object);
    if (!data)
        return NULL;
    const uint32_t* list = *(const uint32_t* const*)(data + 32);
    if (!foundation_archive_readable(list, 8))
        return NULL;
    size_t entry_size = list[0] & 0xffff;
    size_t entry_count = list[1];
    int relative = (list[0] & 0x80000000u) != 0;
    int direct_selector = (list[0] & 0x40000000u) != 0;
    if (entry_size < (relative ? 12u : 24u) || entry_count > MAX_GUEST_METHODS ||
        !foundation_archive_readable(list, 8 + entry_size * entry_count))
        return NULL;
    struct guest_method* result = calloc(entry_count, sizeof(*result));
    if (!result)
        return NULL;
    for (size_t index = 0; index < entry_count; ++index) {
        const unsigned char* entry = (const unsigned char*)list + 8 + index * entry_size;
        const char* selector;
        void* implementation;
        if (relative) {
            const void* selector_address = entry + *(const int32_t*)entry;
            if (!foundation_archive_readable(selector_address,
                    direct_selector ? 1 : sizeof(void*)))
                continue;
            selector = direct_selector ? selector_address : *(const char* const*)selector_address;
            implementation = (void*)(entry + 8 + *(const int32_t*)(entry + 8));
        } else {
            selector = *(const char* const*)entry;
            implementation = *(void* const*)(entry + 16);
        }
        if (!foundation_archive_readable(selector, 1))
            continue;
        result[index] = (struct guest_method){selector, implementation};
    }
    *count = entry_count;
    return result;
}

static struct guest_class* register_class(void* class_object)
{
    struct guest_class* result = find_class(class_object);
    if (result || class_count == MAX_GUEST_CLASSES)
        return result;
    const unsigned char* data = class_data(class_object);
    if (!data)
        return NULL;
    const char* name = *(const char* const*)(data + 24);
    uint32_t instance_size = *(const uint32_t*)(data + 8);
    if (!foundation_archive_readable(name, 1) || instance_size > 64u * 1024u * 1024u)
        return NULL;
    result = &classes[class_count++];
    result->address = class_object;
    result->name = name;
    result->instance_size = instance_size < sizeof(void*) ? sizeof(void*) : instance_size;
    result->instance_methods = parse_methods(class_object, &result->instance_method_count);
    result->class_methods = parse_methods(result->address->isa, &result->class_method_count);
    return result;
}

void machgate_shim_register_objc_class(void* class_object)
{
    pthread_mutex_lock(&classes_mutex);
    register_class(class_object);
    pthread_mutex_unlock(&classes_mutex);
}

static struct guest_class* receiver_class(void* receiver, int* class_method)
{
    struct guest_class* result = find_class(receiver);
    if (result) {
        *class_method = 1;
        return result;
    }
    if (!foundation_archive_readable(receiver, sizeof(void*)))
        return NULL;
    void* isa = *(void**)receiver;
    const unsigned char* data = class_data(isa);
    if (!data)
        return NULL;
    *class_method = (*(const uint32_t*)data & 1u) != 0;
    return register_class(*class_method ? receiver : isa);
}

static void* lookup_chained(struct guest_class* entry, int class_method,
    const char* selector)
{
    for (int depth = 0; depth < 32 && entry; ++depth) {
        struct guest_method* methods = class_method ? entry->class_methods : entry->instance_methods;
        size_t count = class_method ? entry->class_method_count : entry->instance_method_count;
        for (size_t index = 0; index < count; ++index)
            if (methods[index].selector && strcmp(methods[index].selector, selector) == 0)
                return methods[index].implementation;
        entry = register_class(entry->address->superclass);
    }
    return NULL;
}

void* foundation_archive_guest_imp(void* receiver, const char* selector)
{
    if (!receiver || !selector)
        return NULL;
    pthread_mutex_lock(&classes_mutex);
    int class_method = 0;
    struct guest_class* entry = receiver_class(receiver, &class_method);
    void* result = lookup_chained(entry, class_method, selector);
    pthread_mutex_unlock(&classes_mutex);
    return result;
}

int foundation_archive_guest_is_kind_of_class(void* object, void* class_object)
{
    if (!object || !class_object || !foundation_archive_readable(object, sizeof(void*)))
        return 0;
    void* current = *(void**)object;
    pthread_mutex_lock(&classes_mutex);
    int result = 0;
    while (current) {
        if (current == class_object) {
            result = 1;
            break;
        }
        struct guest_class* entry = register_class(current);
        if (!entry)
            break;
        current = entry->address->superclass;
    }
    pthread_mutex_unlock(&classes_mutex);
    return result;
}

void* foundation_archive_super_imp(void* super_data, const char* selector,
    void** receiver)
{
    struct guest_super { void* receiver; struct guest_class_layout* current_class; };
    const struct guest_super* super = super_data;
    if (!super || !selector || !receiver ||
        !foundation_archive_readable(super, sizeof(*super)) ||
        !foundation_archive_readable(super->current_class, sizeof(*super->current_class)))
        return NULL;
    *receiver = super->receiver;
    pthread_mutex_lock(&classes_mutex);
    struct guest_class* entry = register_class(super->current_class->superclass);
    void* result = lookup_chained(entry, 0, selector);
    pthread_mutex_unlock(&classes_mutex);
    return result;
}

void* foundation_archive_guest_class(const char* name)
{
    if (!name)
        return NULL;
    pthread_mutex_lock(&classes_mutex);
    void* result = NULL;
    for (size_t index = 0; index < class_count; ++index)
        if (strcmp(classes[index].name, name) == 0) {
            result = classes[index].address;
            break;
        }
    pthread_mutex_unlock(&classes_mutex);
    return result;
}

void* foundation_archive_guest_alloc(void* class_object)
{
    pthread_mutex_lock(&classes_mutex);
    struct guest_class* entry = register_class(class_object);
    void* result = entry ? calloc(1, entry->instance_size) : NULL;
    if (result)
        *(void**)result = class_object;
    pthread_mutex_unlock(&classes_mutex);
    return result;
}

int foundation_archive_guest_dispatch(void* receiver, const char* selector,
    uintptr_t* result)
{
    pthread_mutex_lock(&classes_mutex);
    int class_method = 0;
    struct guest_class* entry = receiver_class(receiver, &class_method);
    pthread_mutex_unlock(&classes_mutex);
    if (!entry)
        return 0;
    *result = 0;
    if (class_method && (strcmp(selector, "alloc") == 0 ||
        strcmp(selector, "allocWithZone:") == 0 || strcmp(selector, "new") == 0))
        *result = (uintptr_t)foundation_archive_guest_alloc(receiver);
    else if (strcmp(selector, "class") == 0)
        *result = (uintptr_t)entry->address;
    else if (strcmp(selector, "hash") == 0)
        *result = (uintptr_t)receiver;
    else if (strcmp(selector, "init") == 0 || strcmp(selector, "retain") == 0 ||
        strcmp(selector, "autorelease") == 0)
        *result = (uintptr_t)receiver;
    return 1;
}
