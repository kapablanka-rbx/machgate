#define _GNU_SOURCE
#include <dlfcn.h>
#include <link.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define MACHGATE_ICU(name, result, parameters, bridge) \
    __attribute__((visibility("hidden"))) void* machgate_icu_##name;
#include "exports.def"
#undef MACHGATE_ICU

struct host_function {
    const char* symbol;
    void** address;
};

static const struct host_function host_functions[] = {
#define MACHGATE_ICU(name, result, parameters, bridge) \
    {#name "_74", &machgate_icu_##name},
#include "exports.def"
#undef MACHGATE_ICU
};

__attribute__((constructor)) static void initialize_host_runtime(void)
{
    void* runtime = dlmopen(LM_ID_NEWLM, "libicui18n.so.74", RTLD_NOW | RTLD_LOCAL);
    if (!runtime) {
        fprintf(stderr, "machgate ICU: cannot load isolated ICU 74 runtime: %s\n", dlerror());
        abort();
    }

    for (size_t index = 0; index < sizeof(host_functions) / sizeof(host_functions[0]); ++index) {
        const struct host_function* function = &host_functions[index];
        void* result = dlsym(runtime, function->symbol);
        if (!result) {
            fprintf(stderr, "machgate ICU: cannot resolve %s: %s\n", function->symbol, dlerror());
            abort();
        }
        *function->address = result;
    }
}
