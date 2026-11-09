#ifndef MACHGATE_FOUNDATION_ARCHIVE_RUNTIME_H
#define MACHGATE_FOUNDATION_ARCHIVE_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

void* foundation_archive_data_create(const void* bytes, size_t size);
int foundation_archive_data_resolve(const void* object, const void** bytes, size_t* size);
void* foundation_archive_get_class(const char* name);
void* foundation_archive_alloc(void* class_object);
int foundation_archive_dispatch(void* receiver, const char* selector,
    const uintptr_t arguments[6], const uintptr_t* stack, void* indirect_result,
    double floating_argument, uintptr_t* result, double* floating_result);
const char* foundation_archive_string_utf8(const void* object);
void* foundation_archive_guest_imp(void* receiver, const char* selector);
void* foundation_archive_super_imp(void* super_data, const char* selector,
    void** receiver);
void* foundation_archive_guest_class(const char* name);
void* foundation_archive_guest_alloc(void* class_object);
int foundation_archive_guest_is_kind_of_class(void* object, void* class_object);
int foundation_archive_guest_dispatch(void* receiver, const char* selector,
    uintptr_t* result);
int foundation_archive_readable(const void* address, size_t size);
void foundation_archive_forget_object(void* object);
void machgate_shim_register_objc_class(void* class_object);
void machgate_shim_register_objc_category(void* category_object);
void* foundation_archive_category_imp_for_class(const char* class_name,
    int class_method, const char* selector);
const char* foundation_archive_shim_class_name(const void* class_object);
const char* foundation_archive_guest_class_name(const void* class_object);
int foundation_archive_category_dispatch(void* receiver, const char* selector,
    const uintptr_t arguments[6], const uintptr_t* stack, void* indirect_result,
    uintptr_t* result);

#endif
