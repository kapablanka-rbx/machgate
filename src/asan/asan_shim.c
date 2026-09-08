#define _GNU_SOURCE
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <stdio.h>

#include "asan_shim.h"

typedef uintptr_t uptr;
typedef uint64_t u64;
typedef uint32_t u32;

#define ASAN_SHADOW_SCALE 3
#define ASAN_SHADOW_SIZE (16ULL << 30)

static unsigned char* asan_shadow_base = NULL;

uptr __asan_shadow_memory_dynamic_address = 0;
int __asan_option_detect_stack_use_after_return = 0;
uptr* __asan_test_only_reported_buggy_pointer = NULL;

struct __asan_global_source_location {
	const char* filename;
	int line_no;
	int column_no;
};

struct __asan_global {
	uptr beg;
	uptr size;
	uptr size_with_redzone;
	const char* name;
	const char* module_name;
	uptr has_dynamic_init;
	struct __asan_global_source_location* gcc_location;
	uptr odr_indicator;
};

void asan_shim_init(void)
{
	if (asan_shadow_base)
		return;

	void* region = mmap(NULL, ASAN_SHADOW_SIZE, PROT_READ | PROT_WRITE,
	                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
	if (region == MAP_FAILED) {
		fprintf(stderr, "asan_shim: failed to mmap shadow memory\n");
		return;
	}

	asan_shadow_base = (unsigned char*)region;
	__asan_shadow_memory_dynamic_address = (uptr)asan_shadow_base;

	if (getenv("MACHGATE_VERBOSE"))
		fprintf(stderr, "asan_shim: shadow memory at %p (%llu MB)\n",
		        asan_shadow_base, (unsigned long long)(ASAN_SHADOW_SIZE >> 20));
}

static void __attribute__((constructor(110)))
asan_shim_auto_init(void)
{
	asan_shim_init();
}

static void asan_ensure_init(void)
{
	if (!asan_shadow_base)
		asan_shim_init();
}

void __asan_init(void)
{
	asan_ensure_init();
}

void __asan_version_mismatch_check(void) {}

void __asan_before_dynamic_init(const char* module_name) {}
void __asan_after_dynamic_init(void) {}

void __asan_register_globals(struct __asan_global* globals, uptr n) {}
void __asan_unregister_globals(struct __asan_global* globals, uptr n) {}

void __asan_register_image_globals(uptr* flag) {}
void __asan_unregister_image_globals(uptr* flag) {}

void __asan_register_elf_globals(uptr* flag, void* start, void* stop) {}
void __asan_unregister_elf_globals(uptr* flag, void* start, void* stop) {}

static inline void asan_set_shadow(uptr addr, uptr size, unsigned char value)
{
	asan_ensure_init();
	if (!asan_shadow_base)
		return;
	unsigned char* shadow = asan_shadow_base + (addr >> ASAN_SHADOW_SCALE);
	uptr shadow_size = size >> ASAN_SHADOW_SCALE;
	if (shadow_size == 0)
		shadow_size = 1;
	memset(shadow, value, shadow_size);
}

void __asan_set_shadow_00(uptr addr, uptr size) { asan_set_shadow(addr, size, 0x00); }
void __asan_set_shadow_01(uptr addr, uptr size) { asan_set_shadow(addr, size, 0x01); }
void __asan_set_shadow_02(uptr addr, uptr size) { asan_set_shadow(addr, size, 0x02); }
void __asan_set_shadow_03(uptr addr, uptr size) { asan_set_shadow(addr, size, 0x03); }
void __asan_set_shadow_04(uptr addr, uptr size) { asan_set_shadow(addr, size, 0x04); }
void __asan_set_shadow_05(uptr addr, uptr size) { asan_set_shadow(addr, size, 0x05); }
void __asan_set_shadow_06(uptr addr, uptr size) { asan_set_shadow(addr, size, 0x06); }
void __asan_set_shadow_07(uptr addr, uptr size) { asan_set_shadow(addr, size, 0x07); }
void __asan_set_shadow_f1(uptr addr, uptr size) { asan_set_shadow(addr, size, 0xf1); }
void __asan_set_shadow_f2(uptr addr, uptr size) { asan_set_shadow(addr, size, 0xf2); }
void __asan_set_shadow_f3(uptr addr, uptr size) { asan_set_shadow(addr, size, 0xf3); }
void __asan_set_shadow_f5(uptr addr, uptr size) { asan_set_shadow(addr, size, 0xf5); }
void __asan_set_shadow_f8(uptr addr, uptr size) { asan_set_shadow(addr, size, 0xf8); }

void __asan_poison_stack_memory(uptr addr, uptr size)
{
	asan_set_shadow(addr, size, 0xf5);
}

void __asan_unpoison_stack_memory(uptr addr, uptr size)
{
	asan_set_shadow(addr, size, 0x00);
}

void __asan_poison_memory_region(void const volatile* addr, uptr size)
{
	asan_set_shadow((uptr)addr, size, 0xf7);
}

void __asan_unpoison_memory_region(void const volatile* addr, uptr size)
{
	asan_set_shadow((uptr)addr, size, 0x00);
}

void __asan_alloca_poison(uptr addr, uptr size)
{
	asan_set_shadow(addr, size, 0xf2);
}

void __asan_allocas_unpoison(uptr top, uptr bottom)
{
	if (top > bottom) {
		uptr tmp = top;
		top = bottom;
		bottom = tmp;
	}
	asan_set_shadow(top, bottom - top, 0x00);
}

void __asan_load1(uptr p) {}
void __asan_load2(uptr p) {}
void __asan_load4(uptr p) {}
void __asan_load8(uptr p) {}
void __asan_load16(uptr p) {}
void __asan_store1(uptr p) {}
void __asan_store2(uptr p) {}
void __asan_store4(uptr p) {}
void __asan_store8(uptr p) {}
void __asan_store16(uptr p) {}
void __asan_loadN(uptr p, uptr size) {}
void __asan_storeN(uptr p, uptr size) {}

void __asan_load1_noabort(uptr p) {}
void __asan_load2_noabort(uptr p) {}
void __asan_load4_noabort(uptr p) {}
void __asan_load8_noabort(uptr p) {}
void __asan_load16_noabort(uptr p) {}
void __asan_store1_noabort(uptr p) {}
void __asan_store2_noabort(uptr p) {}
void __asan_store4_noabort(uptr p) {}
void __asan_store8_noabort(uptr p) {}
void __asan_store16_noabort(uptr p) {}
void __asan_loadN_noabort(uptr p, uptr size) {}
void __asan_storeN_noabort(uptr p, uptr size) {}

void __asan_exp_load1(uptr p, u32 exp) {}
void __asan_exp_load2(uptr p, u32 exp) {}
void __asan_exp_load4(uptr p, u32 exp) {}
void __asan_exp_load8(uptr p, u32 exp) {}
void __asan_exp_load16(uptr p, u32 exp) {}
void __asan_exp_store1(uptr p, u32 exp) {}
void __asan_exp_store2(uptr p, u32 exp) {}
void __asan_exp_store4(uptr p, u32 exp) {}
void __asan_exp_store8(uptr p, u32 exp) {}
void __asan_exp_store16(uptr p, u32 exp) {}
void __asan_exp_loadN(uptr p, uptr size, u32 exp) {}
void __asan_exp_storeN(uptr p, uptr size, u32 exp) {}

void __asan_handle_no_return(void) {}
void __asan_handle_vfork(void* sp) {}

void __asan_poison_cxx_array_cookie(uptr p) {}
uptr __asan_load_cxx_array_cookie(uptr* p) { return *p; }
void __asan_poison_intra_object_redzone(uptr p, uptr size) {}
void __asan_unpoison_intra_object_redzone(uptr p, uptr size) {}

void* __asan_memcpy(void* dst, const void* src, uptr size)
{
	return memcpy(dst, src, size);
}

void* __asan_memset(void* s, int c, uptr n)
{
	return memset(s, c, n);
}

void* __asan_memmove(void* dest, const void* src, uptr n)
{
	return memmove(dest, src, n);
}

int __asan_address_is_poisoned(void const volatile* addr) { return 0; }
uptr __asan_region_is_poisoned(uptr beg, uptr size) { return 0; }

void __asan_describe_address(uptr addr) {}
int __asan_report_present(void) { return 0; }
uptr __asan_get_report_pc(void) { return 0; }
uptr __asan_get_report_bp(void) { return 0; }
uptr __asan_get_report_sp(void) { return 0; }
uptr __asan_get_report_address(void) { return 0; }
int __asan_get_report_access_type(void) { return 0; }
uptr __asan_get_report_access_size(void) { return 0; }
int __asan_get_report_src_address(uptr* out_addr, uptr* out_size) { return 0; }
int __asan_get_report_dest_address(uptr* out_addr, uptr* out_size) { return 0; }
int __asan_get_report_dealloc_address(uptr* out_addr, uptr* out_size) { return 0; }
int __asan_get_report_first_address(uptr* out_addr, uptr* out_size) { return 0; }
int __asan_get_report_second_address(uptr* out_addr, uptr* out_size) { return 0; }
const char* __asan_get_report_description(void) { return ""; }

void __asan_report_error(uptr pc, uptr bp, uptr sp,
                         uptr addr, int is_write, uptr access_size, u32 exp)
{
	fprintf(stderr, "asan_shim: report_error addr=%p size=%zu\n",
	        (void*)addr, access_size);
}

const char* __asan_locate_address(uptr addr, char* name, uptr name_size,
                                  uptr* region_address, uptr* region_size)
{
	if (name && name_size > 0)
		name[0] = '\0';
	if (region_address) *region_address = 0;
	if (region_size) *region_size = 0;
	return "";
}

uptr __asan_get_alloc_stack(uptr addr, uptr* trace, uptr size, u32* thread_id) { return 0; }
uptr __asan_get_free_stack(uptr addr, uptr* trace, uptr size, u32* thread_id) { return 0; }

void __asan_get_shadow_mapping(uptr* shadow_scale, uptr* shadow_offset)
{
	if (shadow_scale) *shadow_scale = ASAN_SHADOW_SCALE;
	if (shadow_offset) *shadow_offset = (uptr)asan_shadow_base;
}

void __asan_set_death_callback(void (*callback)(void)) {}
void __asan_set_error_report_callback(void (*callback)(const char*)) {}
void __asan_on_error(void) {}
void __asan_print_accumulated_stats(void) {}
const char* __asan_default_options(void) { return ""; }
const char* __asan_default_suppressions(void) { return ""; }
int __asan_update_allocation_context(void* addr) { return 0; }

void __asan_report_load1(uptr p) {}
void __asan_report_load2(uptr p) {}
void __asan_report_load4(uptr p) {}
void __asan_report_load8(uptr p) {}
void __asan_report_load16(uptr p) {}
void __asan_report_load_n(uptr p, uptr size) {}
void __asan_report_store1(uptr p) {}
void __asan_report_store2(uptr p) {}
void __asan_report_store4(uptr p) {}
void __asan_report_store8(uptr p) {}
void __asan_report_store16(uptr p) {}
void __asan_report_store_n(uptr p, uptr size) {}

void __asan_version_mismatch_check_v8(void) {}

void __sanitizer_annotate_contiguous_container(void* beg, void* end,
    void* old_mid, void* new_mid) {}
void __sanitizer_finish_switch_fiber(void* fake_stack_save) {}
void __sanitizer_start_switch_fiber(void** fake_stack_save, void* bottom,
    uptr size) {}

void* __asan_stack_malloc_0(uptr size) { return (void*)0; }
void* __asan_stack_malloc_1(uptr size) { return (void*)0; }
void* __asan_stack_malloc_2(uptr size) { return (void*)0; }
void* __asan_stack_malloc_3(uptr size) { return (void*)0; }
void* __asan_stack_malloc_4(uptr size) { return (void*)0; }
void* __asan_stack_malloc_5(uptr size) { return (void*)0; }
void* __asan_stack_malloc_6(uptr size) { return (void*)0; }
void* __asan_stack_malloc_7(uptr size) { return (void*)0; }
void* __asan_stack_malloc_8(uptr size) { return (void*)0; }
void* __asan_stack_malloc_9(uptr size) { return (void*)0; }
void* __asan_stack_malloc_10(uptr size) { return (void*)0; }

void __asan_stack_free_0(uptr p, uptr size) {}
void __asan_stack_free_1(uptr p, uptr size) {}
void __asan_stack_free_2(uptr p, uptr size) {}
void __asan_stack_free_3(uptr p, uptr size) {}
void __asan_stack_free_4(uptr p, uptr size) {}
void __asan_stack_free_5(uptr p, uptr size) {}
void __asan_stack_free_6(uptr p, uptr size) {}
void __asan_stack_free_7(uptr p, uptr size) {}
void __asan_stack_free_8(uptr p, uptr size) {}
void __asan_stack_free_9(uptr p, uptr size) {}
void __asan_stack_free_10(uptr p, uptr size) {}
