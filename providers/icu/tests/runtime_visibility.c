#include <dlfcn.h>
#include <stdio.h>

int main(int argc, char** argv)
{
    if (argc != 2)
        return 1;

    const char* exception_runtime_symbol = "_ZTVN10__cxxabiv120__si_class_type_infoE";
    void* original_exception_runtime = dlsym(RTLD_DEFAULT, exception_runtime_symbol);
    void* provider = dlopen(argv[1], RTLD_NOW | RTLD_GLOBAL);
    if (!provider) {
        fprintf(stderr, "cannot load ICU provider: %s\n", dlerror());
        return 2;
    }

    int (*uppercase)(int) = dlsym(provider, "u_toupper");
    if (!uppercase || uppercase(0x00e4) != 0x00c4)
        return 3;

    if (dlsym(RTLD_DEFAULT, exception_runtime_symbol) != original_exception_runtime) {
        fprintf(stderr, "ICU changed the process-global C++ exception runtime\n");
        return 4;
    }
    return 0;
}
