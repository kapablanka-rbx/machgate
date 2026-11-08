#include <dlfcn.h>
#include <stdio.h>

static const char* exports[] = {
#define MACHGATE_ICU(name, result, parameters, bridge) #name,
#include "../exports.def"
#undef MACHGATE_ICU
};

int main(int argc, char** argv)
{
    if (argc != 2)
        return 1;
    void* provider = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!provider) {
        fprintf(stderr, "%s\n", dlerror());
        return 2;
    }
    for (size_t index = 0; index < sizeof(exports) / sizeof(exports[0]); index++) {
        if (!dlsym(provider, exports[index])) {
            fprintf(stderr, "missing ICU symbol: %s\n", exports[index]);
            return 3;
        }
    }
    dlclose(provider);
    printf("ICU export lookup: %zu names passed\n", sizeof(exports) / sizeof(exports[0]));
    return 0;
}
