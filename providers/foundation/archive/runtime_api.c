#include "runtime.h"
#include "../string_value.h"

#include <pthread.h>
#include <stdlib.h>

struct weak_reference {
    void** location;
    struct weak_reference* next;
};

static struct weak_reference* weak_references;
static pthread_mutex_t weak_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t property_mutex = PTHREAD_MUTEX_INITIALIZER;

static struct weak_reference** weak_slot(void** location)
{
    struct weak_reference** result = &weak_references;
    while (*result && (*result)->location != location)
        result = &(*result)->next;
    return result;
}

void* objc_storeWeak(void** location, void* object)
{
    if (!location)
        return object;
    pthread_mutex_lock(&weak_mutex);
    struct weak_reference** slot = weak_slot(location);
    if (!*slot && object) {
        *slot = malloc(sizeof(**slot));
        if (!*slot) {
            *location = NULL;
            pthread_mutex_unlock(&weak_mutex);
            return NULL;
        }
        **slot = (struct weak_reference){location, NULL};
    }
    *location = object;
    pthread_mutex_unlock(&weak_mutex);
    return object;
}

void* objc_initWeak(void** location, void* object)
{
    return objc_storeWeak(location, object);
}

void* objc_loadWeakRetained(void** location)
{
    if (!location)
        return NULL;
    pthread_mutex_lock(&weak_mutex);
    void* result = foundation_retain(*location);
    pthread_mutex_unlock(&weak_mutex);
    return result;
}

void* objc_loadWeak(void** location)
{
    return foundation_autorelease(objc_loadWeakRetained(location));
}

void objc_destroyWeak(void** location)
{
    if (!location)
        return;
    pthread_mutex_lock(&weak_mutex);
    struct weak_reference** slot = weak_slot(location);
    if (*slot) {
        struct weak_reference* removed = *slot;
        *slot = removed->next;
        free(removed);
    }
    *location = NULL;
    pthread_mutex_unlock(&weak_mutex);
}

void objc_copyWeak(void** destination, void** source)
{
    void* object = objc_loadWeakRetained(source);
    objc_initWeak(destination, object);
    foundation_release(object);
}

void foundation_archive_forget_object(void* object)
{
    pthread_mutex_lock(&weak_mutex);
    for (struct weak_reference* reference = weak_references; reference; reference = reference->next)
        if (*reference->location == object)
            *reference->location = NULL;
    pthread_mutex_unlock(&weak_mutex);
}

void objc_storeStrong(void** location, void* object)
{
    if (!location)
        return;
    void* previous = *location;
    *location = foundation_retain(object);
    foundation_release(previous);
}

void* objc_getProperty(void* receiver, void* selector, uintptr_t offset, int atomic)
{
    (void)selector;
    if (!receiver)
        return NULL;
    void** location = (void**)((char*)receiver + offset);
    if (!atomic)
        return *location;
    pthread_mutex_lock(&property_mutex);
    void* result = foundation_retain(*location);
    pthread_mutex_unlock(&property_mutex);
    return foundation_autorelease(result);
}

void objc_setProperty_atomic(void* receiver, void* selector, void* value, uintptr_t offset)
{
    (void)selector;
    if (!receiver)
        return;
    pthread_mutex_lock(&property_mutex);
    objc_storeStrong((void**)((char*)receiver + offset), value);
    pthread_mutex_unlock(&property_mutex);
}

void objc_setProperty_nonatomic(void* receiver, void* selector, void* value, uintptr_t offset)
{
    (void)selector;
    if (receiver)
        objc_storeStrong((void**)((char*)receiver + offset), value);
}

void objc_setProperty_nonatomic_copy(void* receiver, void* selector, void* value, uintptr_t offset)
{
    (void)selector;
    if (!receiver)
        return;
    extern void* objc_msgSend(void*, const char*);
    void* copy = value ? objc_msgSend(value, "copy") : NULL;
    void** location = (void**)((char*)receiver + offset);
    void* previous = *location;
    *location = copy;
    foundation_release(previous);
}

void* objc_retainBlock(void* block)
{
    void* (*copy)(void*) = foundation_shim_symbol("_Block_copy");
    return copy ? copy(block) : block;
}

void objc_enumerationMutation(void* object)
{
    (void)object;
    abort();
}
