#include <dlfcn.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <unwind.h>

#include "archive/runtime.h"

struct objc_typeinfo {
    const void** vtable;
    const char* name;
    void* class_object;
};

extern const void* objc_ehtype_vtable[];
struct objc_typeinfo OBJC_EHTYPE_id = {objc_ehtype_vtable + 2, "id", NULL};
struct objc_typeinfo OBJC_EHTYPE_$_NSException = {
    objc_ehtype_vtable + 2, "NSException", NULL};

_Static_assert(sizeof(struct objc_typeinfo) == 24, "Darwin Objective-C typeinfo ABI");
_Static_assert(offsetof(struct objc_typeinfo, class_object) == 16, "Darwin catch class offset");

static void exception_typeinfo_noop(void) {}
static bool exception_typeinfo_false(void) { return false; }

static bool exception_typeinfo_can_catch(const struct objc_typeinfo* handler,
    const struct objc_typeinfo* thrown, void** object)
{
    if (thrown->vtable != objc_ehtype_vtable + 2)
        return false;
    *object = **(void***)object;
    if (handler == &OBJC_EHTYPE_id)
        return true;
    return foundation_archive_guest_is_kind_of_class(*object, handler->class_object);
}

const void* objc_ehtype_vtable[] = {
    NULL, &OBJC_EHTYPE_id,
    exception_typeinfo_noop, exception_typeinfo_noop,
    exception_typeinfo_noop, exception_typeinfo_noop,
    exception_typeinfo_can_catch,
    exception_typeinfo_false, exception_typeinfo_false,
    abort, abort, abort, abort,
};

static void* cxx_exception_function(const char* name)
{
    void* result = dlsym(RTLD_DEFAULT, name);
    if (!result)
        abort();
    return result;
}

_Unwind_Reason_Code __objc_personality_v0(int version, _Unwind_Action actions,
    uint64_t exception_class, struct _Unwind_Exception* exception,
    struct _Unwind_Context* context)
{
    typedef _Unwind_Reason_Code (*personality_function)(int, _Unwind_Action,
        uint64_t, struct _Unwind_Exception*, struct _Unwind_Context*);
    personality_function personality = cxx_exception_function("__gxx_personality_v0");
    return personality(version, actions, exception_class, exception, context);
}

void* objc_begin_catch(void* exception)
{
    typedef void* (*begin_catch_function)(void*);
    begin_catch_function begin_catch = cxx_exception_function("__cxa_begin_catch");
    return begin_catch(exception);
}

void objc_end_catch(void)
{
    typedef void (*end_catch_function)(void);
    end_catch_function end_catch = cxx_exception_function("__cxa_end_catch");
    end_catch();
}

_Noreturn void objc_exception_rethrow(void)
{
    typedef void (*rethrow_function)(void);
    rethrow_function rethrow = cxx_exception_function("__cxa_rethrow");
    rethrow();
    abort();
}

_Noreturn void objc_terminate(void)
{
    typedef void (*terminate_function)(void);
    terminate_function terminate = cxx_exception_function("_ZSt9terminatev");
    terminate();
    abort();
}
