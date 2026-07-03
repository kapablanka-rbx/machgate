#include <stddef.h>
#include <stdlib.h>

void* machgate_shim_malloc(size_t size);
void* machgate_shim_calloc(size_t count, size_t size);
void* machgate_shim_realloc(void* ptr, size_t size);
void machgate_shim_free(void* ptr);
int machgate_shim_posix_memalign(void** memptr, size_t alignment, size_t size);
void* machgate_shim_memalign(size_t alignment, size_t size);
void* machgate_shim_valloc(size_t size);
void* machgate_shim_guest_operator_new(size_t size);
void* machgate_shim_guest_operator_new_array(size_t size);
void* machgate_shim_guest_operator_new_aligned(size_t size, size_t alignment);
void* machgate_shim_guest_operator_new_array_aligned(size_t size, size_t alignment);
void* machgate_shim_guest_operator_new_nothrow(size_t size, const void* nothrow_arg);
void* machgate_shim_guest_operator_new_array_nothrow(size_t size, const void* nothrow_arg);
void* machgate_shim_guest_operator_new_aligned_nothrow(size_t size, size_t alignment, const void* nothrow_arg);
void* machgate_shim_guest_operator_new_array_aligned_nothrow(size_t size, size_t alignment, const void* nothrow_arg);
void machgate_shim_guest_operator_delete(void* ptr);
void machgate_shim_guest_operator_delete_array(void* ptr);
void machgate_shim_guest_operator_delete_sized(void* ptr, size_t size);
void machgate_shim_guest_operator_delete_array_sized(void* ptr, size_t size);
void machgate_shim_guest_operator_delete_aligned(void* ptr, size_t alignment);
void machgate_shim_guest_operator_delete_array_aligned(void* ptr, size_t alignment);
void machgate_shim_guest_operator_delete_sized_aligned(void* ptr, size_t size, size_t alignment);
void machgate_shim_guest_operator_delete_array_sized_aligned(void* ptr, size_t size, size_t alignment);
void machgate_shim_guest_operator_delete_nothrow(void* ptr, const void* nothrow_arg);
void machgate_shim_guest_operator_delete_array_nothrow(void* ptr, const void* nothrow_arg);
void machgate_shim_guest_operator_delete_aligned_nothrow(void* ptr, size_t alignment, const void* nothrow_arg);
void machgate_shim_guest_operator_delete_array_aligned_nothrow(void* ptr, size_t alignment, const void* nothrow_arg);

void* __wrap_malloc(size_t size)
{
	return machgate_shim_malloc(size);
}

void __wrap_free(void* ptr)
{
	machgate_shim_free(ptr);
}

void* __wrap_calloc(size_t count, size_t size)
{
	return machgate_shim_calloc(count, size);
}

void* __wrap_realloc(void* ptr, size_t size)
{
	return machgate_shim_realloc(ptr, size);
}

int __wrap_posix_memalign(void** memptr, size_t alignment, size_t size)
{
	return machgate_shim_posix_memalign(memptr, alignment, size);
}

void* __wrap_memalign(size_t alignment, size_t size)
{
	return machgate_shim_memalign(alignment, size);
}

void* __wrap_aligned_alloc(size_t alignment, size_t size)
{
	return machgate_shim_memalign(alignment, size);
}

void* __wrap_valloc(size_t size)
{
	return machgate_shim_valloc(size);
}

void* libcxx_operator_new(size_t size) __asm__("__wrap__Znwm");
void* libcxx_operator_new(size_t size)
{
	return machgate_shim_guest_operator_new(size);
}

void* libcxx_operator_new_array(size_t size) __asm__("__wrap__Znam");
void* libcxx_operator_new_array(size_t size)
{
	return machgate_shim_guest_operator_new_array(size);
}

void* libcxx_operator_new_nothrow(size_t size, const void* nothrow_arg) __asm__("__wrap__ZnwmRKSt9nothrow_t");
void* libcxx_operator_new_nothrow(size_t size, const void* nothrow_arg)
{
	return machgate_shim_guest_operator_new_nothrow(size, nothrow_arg);
}

void* libcxx_operator_new_array_nothrow(size_t size, const void* nothrow_arg) __asm__("__wrap__ZnamRKSt9nothrow_t");
void* libcxx_operator_new_array_nothrow(size_t size, const void* nothrow_arg)
{
	return machgate_shim_guest_operator_new_array_nothrow(size, nothrow_arg);
}

void* libcxx_operator_new_aligned(size_t size, size_t alignment) __asm__("__wrap__ZnwmSt11align_val_t");
void* libcxx_operator_new_aligned(size_t size, size_t alignment)
{
	return machgate_shim_guest_operator_new_aligned(size, alignment);
}

void* libcxx_operator_new_array_aligned(size_t size, size_t alignment) __asm__("__wrap__ZnamSt11align_val_t");
void* libcxx_operator_new_array_aligned(size_t size, size_t alignment)
{
	return machgate_shim_guest_operator_new_array_aligned(size, alignment);
}

void* libcxx_operator_new_aligned_nothrow(size_t size, size_t alignment, const void* nothrow_arg) __asm__("__wrap__ZnwmSt11align_val_tRKSt9nothrow_t");
void* libcxx_operator_new_aligned_nothrow(size_t size, size_t alignment, const void* nothrow_arg)
{
	return machgate_shim_guest_operator_new_aligned_nothrow(size, alignment,
	                                                        nothrow_arg);
}

void* libcxx_operator_new_array_aligned_nothrow(size_t size, size_t alignment, const void* nothrow_arg) __asm__("__wrap__ZnamSt11align_val_tRKSt9nothrow_t");
void* libcxx_operator_new_array_aligned_nothrow(size_t size, size_t alignment, const void* nothrow_arg)
{
	return machgate_shim_guest_operator_new_array_aligned_nothrow(size,
	                                                              alignment,
	                                                              nothrow_arg);
}

void libcxx_operator_delete(void* ptr) __asm__("__wrap__ZdlPv");
void libcxx_operator_delete(void* ptr)
{
	machgate_shim_guest_operator_delete(ptr);
}

void libcxx_operator_delete_array(void* ptr) __asm__("__wrap__ZdaPv");
void libcxx_operator_delete_array(void* ptr)
{
	machgate_shim_guest_operator_delete_array(ptr);
}

void libcxx_operator_delete_sized(void* ptr, size_t size) __asm__("__wrap__ZdlPvm");
void libcxx_operator_delete_sized(void* ptr, size_t size)
{
	machgate_shim_guest_operator_delete_sized(ptr, size);
}

void libcxx_operator_delete_array_sized(void* ptr, size_t size) __asm__("__wrap__ZdaPvm");
void libcxx_operator_delete_array_sized(void* ptr, size_t size)
{
	machgate_shim_guest_operator_delete_array_sized(ptr, size);
}

void libcxx_operator_delete_nothrow(void* ptr, const void* nothrow_arg) __asm__("__wrap__ZdlPvRKSt9nothrow_t");
void libcxx_operator_delete_nothrow(void* ptr, const void* nothrow_arg)
{
	machgate_shim_guest_operator_delete_nothrow(ptr, nothrow_arg);
}

void libcxx_operator_delete_array_nothrow(void* ptr, const void* nothrow_arg) __asm__("__wrap__ZdaPvRKSt9nothrow_t");
void libcxx_operator_delete_array_nothrow(void* ptr, const void* nothrow_arg)
{
	machgate_shim_guest_operator_delete_array_nothrow(ptr, nothrow_arg);
}

void libcxx_operator_delete_aligned(void* ptr, size_t alignment) __asm__("__wrap__ZdlPvSt11align_val_t");
void libcxx_operator_delete_aligned(void* ptr, size_t alignment)
{
	machgate_shim_guest_operator_delete_aligned(ptr, alignment);
}

void libcxx_operator_delete_array_aligned(void* ptr, size_t alignment) __asm__("__wrap__ZdaPvSt11align_val_t");
void libcxx_operator_delete_array_aligned(void* ptr, size_t alignment)
{
	machgate_shim_guest_operator_delete_array_aligned(ptr, alignment);
}

void libcxx_operator_delete_sized_aligned(void* ptr, size_t size, size_t alignment) __asm__("__wrap__ZdlPvmSt11align_val_t");
void libcxx_operator_delete_sized_aligned(void* ptr, size_t size, size_t alignment)
{
	machgate_shim_guest_operator_delete_sized_aligned(ptr, size, alignment);
}

void libcxx_operator_delete_array_sized_aligned(void* ptr, size_t size, size_t alignment) __asm__("__wrap__ZdaPvmSt11align_val_t");
void libcxx_operator_delete_array_sized_aligned(void* ptr, size_t size, size_t alignment)
{
	machgate_shim_guest_operator_delete_array_sized_aligned(ptr, size, alignment);
}

void libcxx_operator_delete_aligned_nothrow(void* ptr, size_t alignment, const void* nothrow_arg) __asm__("__wrap__ZdlPvSt11align_val_tRKSt9nothrow_t");
void libcxx_operator_delete_aligned_nothrow(void* ptr, size_t alignment, const void* nothrow_arg)
{
	machgate_shim_guest_operator_delete_aligned_nothrow(ptr, alignment,
	                                                    nothrow_arg);
}

void libcxx_operator_delete_array_aligned_nothrow(void* ptr, size_t alignment, const void* nothrow_arg) __asm__("__wrap__ZdaPvSt11align_val_tRKSt9nothrow_t");
void libcxx_operator_delete_array_aligned_nothrow(void* ptr, size_t alignment, const void* nothrow_arg)
{
	machgate_shim_guest_operator_delete_array_aligned_nothrow(ptr, alignment,
	                                                          nothrow_arg);
}
