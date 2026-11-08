#ifndef MACHGATE_ALLOCATION_LEDGER_H
#define MACHGATE_ALLOCATION_LEDGER_H

#include <stddef.h>

#define MACHGATE_ALLOCATION_GUEST_CXX 1u

__attribute__((visibility("hidden")))
void machgate_record_allocation_with_zone(void* ptr, size_t size, void* zone);

__attribute__((visibility("hidden")))
void machgate_record_allocation(void* ptr, size_t size);

__attribute__((visibility("hidden")))
int machgate_lookup_allocation(const void* ptr, size_t* size_out, void** zone_out, unsigned* flags_out);

__attribute__((visibility("hidden")))
int machgate_lookup_allocation_size(const void* ptr, size_t* size_out);

__attribute__((visibility("hidden")))
size_t machgate_recorded_allocation_size(void* ptr, size_t requested_size);

__attribute__((visibility("hidden")))
int machgate_take_allocation(const void* ptr, size_t* size_out);

__attribute__((visibility("hidden")))
void machgate_forget_allocation(const void* ptr);

__attribute__((visibility("hidden")))
void machgate_mark_guest_cxx_allocation(void* ptr, size_t size);

#endif
