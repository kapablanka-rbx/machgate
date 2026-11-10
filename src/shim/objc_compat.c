#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct machgate_generic_object {
	uintptr_t isa;
};

static struct machgate_generic_object generic_class;

void* OBJC_CLASS_$_MachGateGenericObject = &generic_class;
void* OBJC_CLASS_$_NSObject = &generic_class;
void* OBJC_METACLASS_$_NSObject = &generic_class;
void* OBJC_CLASS_$_NSProcessInfo = &generic_class;
void* OBJC_CLASS_$_NSString = &generic_class;
void* OBJC_CLASS_$_NSBundle = &generic_class;
void* OBJC_CLASS_$_NSAutoreleasePool = &generic_class;
void* OBJC_CLASS_$_NSNotificationCenter = &generic_class;
void* OBJC_CLASS_$_NSAssertionHandler = &generic_class;
void* OBJC_CLASS_$_NSDistributedNotificationCenter = &generic_class;
void* OBJC_CLASS_$_NSHTTPCookie = &generic_class;
void* OBJC_CLASS_$_NSHTTPCookieStorage = &generic_class;
void* OBJC_CLASS_$_NSNumber = &generic_class;
void* OBJC_CLASS_$_NSOperationQueue = &generic_class;
void* OBJC_CLASS_$_NSTask = &generic_class;
void* OBJC_CLASS_$_NSThread = &generic_class;
void* OBJC_CLASS_$_NSArray = &generic_class;
void* OBJC_CLASS_$_NSDictionary = &generic_class;
void* OBJC_CLASS_$_NSMutableArray = &generic_class;
void* OBJC_CLASS_$_NSMutableDictionary = &generic_class;
void* OBJC_CLASS_$_NSMutableOrderedSet = &generic_class;
void* OBJC_CLASS_$_NSRunLoop = &generic_class;
void* OBJC_CLASS_$_NSSet = &generic_class;
void* OBJC_CLASS_$_NSURL = &generic_class;
void* OBJC_EHTYPE_$_NSException = &generic_class;
void* _objc_empty_cache = &generic_class;
void* _objc_ehtype_vtable = &generic_class;
void* _NSProcessInfoThermalStateDidChangeNotification = &generic_class;
void* _NSRunLoopCommonModes = &generic_class;

static struct machgate_generic_object generic_instance;

static int objc_trace_enabled(void)
{
	static int cached = -1;
	if (cached < 0) {
		const char* value = getenv("MACHGATE_TRACE_OBJC_MSGSEND");
		cached = value && *value && strcmp(value, "0") != 0;
	}
	return cached;
}

void* objc_msgSend(void* receiver, void* selector, ...)
{
	if (!receiver || !selector)
		return NULL;
	const char* name = selector;

	if (strcmp(name, "alloc") == 0 || strcmp(name, "new") == 0)
		return &generic_instance;
	if (strcmp(name, "init") == 0 || strcmp(name, "retain") == 0 ||
	    strcmp(name, "autorelease") == 0 || strcmp(name, "copy") == 0 ||
	    strcmp(name, "mutableCopy") == 0 || strcmp(name, "self") == 0 ||
	    strcmp(name, "description") == 0)
		return receiver;
	if (strcmp(name, "release") == 0)
		return NULL;
	if (strcmp(name, "count") == 0)
		return (void*)1;
	if (strcmp(name, "hash") == 0)
		return (void*)(uintptr_t)receiver;
	if (strcmp(name, "class") == 0)
		return receiver;
	if (strcmp(name, "respondsToSelector:") == 0 ||
	    strcmp(name, "isKindOfClass:") == 0)
		return (void*)0;
	if (strcmp(name, "length") == 0)
		return (void*)0;

	if (objc_trace_enabled())
		fprintf(stderr, "objc_compat: unhandled selector '%s' on %p\n",
			name, receiver);

	return receiver;
}

void* objc_getClass(const char* name)
{
	return name ? &generic_class : NULL;
}

void* objc_lookUpClass(const char* name)
{
	return objc_getClass(name);
}

void* objc_alloc(void* class_object)
{
	return class_object ? &generic_instance : NULL;
}

void* objc_allocWithZone(void* class_object, void* zone)
{
	(void)zone;
	return objc_alloc(class_object);
}

void* objc_retain(void* object)
{
	return object;
}

void objc_release(void* object)
{
	(void)object;
}

void* objc_autorelease(void* object)
{
	return object;
}

void* objc_retainAutoreleasedReturnValue(void* object)
{
	return object;
}

void* objc_retainAutorelease(void* object)
{
	return object;
}

void* objc_retainAutoreleaseReturnValue(void* object)
{
	return object;
}

void* objc_autoreleaseReturnValue(void* object)
{
	return object;
}

void* objc_initWeak(void** location, void* object)
{
	if (location)
		*location = object;
	return object;
}

void* objc_storeWeak(void** location, void* object)
{
	if (location)
		*location = object;
	return object;
}

void* objc_loadWeakRetained(void** location)
{
	return location ? *location : NULL;
}

void* objc_loadWeak(void** location)
{
	return location ? *location : NULL;
}

void objc_destroyWeak(void** location)
{
	if (location)
		*location = NULL;
}

void objc_copyWeak(void** destination, void** source)
{
	if (destination)
		*destination = source ? *source : NULL;
}

void objc_storeStrong(void** location, void* object)
{
	if (location)
		*location = object;
}

void objc_enumerationMutation(void* object)
{
	(void)object;
}

void* objc_retainBlock(void* block)
{
	return block;
}

void* objc_getProperty(void* receiver, void* selector, uintptr_t offset,
		       int atomic)
{
	(void)selector;
	(void)atomic;
	if (!receiver)
		return NULL;
	return *(void**)((char*)receiver + offset);
}

void objc_setProperty_atomic(void* receiver, void* selector, void* value,
			     uintptr_t offset)
{
	(void)selector;
	if (receiver)
		*(void**)((char*)receiver + offset) = value;
}

void objc_setProperty_nonatomic(void* receiver, void* selector, void* value,
				uintptr_t offset)
{
	(void)selector;
	if (receiver)
		*(void**)((char*)receiver + offset) = value;
}

void objc_setProperty_nonatomic_copy(void* receiver, void* selector,
				     void* value, uintptr_t offset)
{
	(void)selector;
	if (receiver)
		*(void**)((char*)receiver + offset) = value;
}
