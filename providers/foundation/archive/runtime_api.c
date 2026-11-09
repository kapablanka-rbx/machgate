#include "runtime.h"
#include "../string_value.h"

#include <pthread.h>
#include <stdlib.h>

int machgate_keychain_dictionary_lookup_v1(const void* dictionary,
	const void* key, const char** string_value, const void** data_bytes,
	size_t* data_length)
{
	if (string_value)
		*string_value = NULL;
	if (data_bytes)
		*data_bytes = NULL;
	if (data_length)
		*data_length = 0;

	uintptr_t arguments[6] = {(uintptr_t)key};
	uintptr_t value = 0;
	double floating_result;
	if (!foundation_archive_dispatch((void*)dictionary, "objectForKey:",
	    arguments, NULL, NULL, 0, &value, &floating_result) || !value)
		return 0;

	const char* text = foundation_archive_string_utf8((void*)value);
	if (!text) {
		struct foundation_string* string =
			foundation_string_resolve((void*)value);
		text = string ? string->utf8 : NULL;
	}
	if (text && string_value) {
		*string_value = text;
		return 1;
	}
	if (!text && foundation_archive_data_resolve((void*)value, data_bytes,
	    data_length))
		return 1;
	return text != NULL;
}

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
